#!/usr/bin/env python3
"""Verify a 4 KiB AHCI save, flush and fresh-boot readback on a generated Q35 disk."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import socket
import stat
import subprocess
import tempfile
import time
from types import SimpleNamespace

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from exfat_test_image import ExfatFixture, check_clean_image
from hardware_boot import EMPTY_INITRAMFS
from installation_image import digest
from installed_disk_smoke import data_image, outside_data, partitions, save_json
from mk_usb_image import build_image
from pack_runtime_elf import runtime_elf
import source_provenance
from uefi_usb_smoke import default_ovmf_path

ROOT = Path(__file__).resolve().parents[1]
DESTINATION = "data/persist.bin"
CLEAN = rb"exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def make_disk(out):
    kernel = ROOT / "build/kernel/srvros.elf"
    dd = ROOT / "build/userspace/dd.elf"
    sync = ROOT / "build/userspace/posixutils.elf"
    loader = ROOT / "build/limine-binary/BOOTX64.EFI"
    imports = {str(path.relative_to(ROOT)): digest(path) for path in (kernel, dd, sync, loader)}
    boot_kernel = out / "srvros.elf"
    subprocess.run(("objcopy", "--strip-debug", kernel, boot_kernel), check=True)
    seed = hashlib.shake_256(b"SrvrOS file-backed AHCI persistence").hexdigest(2048).encode()
    volume = out / "initial.exfat"
    with volume.open("xb") as stream:
        stream.truncate(32 * 1024 * 1024)
    subprocess.run(("mkfs.exfat", "-c", "4K", volume), check=True, stdout=subprocess.DEVNULL)
    fixture = ExfatFixture(volume.read_bytes())
    fixture.add_directory_files("bin", {"dd": runtime_elf(dd.read_bytes()), "sync": runtime_elf(sync.read_bytes())})
    fixture.add_directory_file("data", "source.bin", seed)
    volume.write_bytes(fixture.data)
    (out / "fsck-initial.log").write_text(check_clean_image(volume))

    config = out / "limine.conf"
    config.write_text("timeout: 0\nserial: yes\n\n/SrvrOS file-backed AHCI persistence\n"
                      "    protocol: limine\n    path: boot():/boot/srvros.elf\n"
                      "    module_path: boot():/boot/initramfs.tar\n    cmdline: srvros.log=debug\n")
    initramfs = out / "initramfs.tar"
    initramfs.write_bytes(EMPTY_INITRAMFS)
    disk = out / "ssd.raw"
    build_image(SimpleNamespace(esp_size_mib=272, exfat=volume, bootx64=loader,
                                bootia32=None, kernel=boot_kernel, initramfs=initramfs,
                                config=config, output=disk))
    return disk, seed, imports


def block_stats(qmp):
    rows = [item["stats"] for item in qmp.call("query-blockstats") if item.get("device") == "testssd"]
    require(len(rows) == 1, "Expected exactly one target SSD block backend")
    stats, = rows
    require(all(stats.get(f"failed_{kind}_operations", 0) == 0 for kind in ("rd", "wr", "flush")),
            "QEMU reported a failed SSD request")
    return stats


def ata_commands(trace, start, end=None):
    return [int(opcode, 16) for opcode in re.findall(rb"ide_bus_exec_cmd [^\n]+ cmd (0x[\da-f]+)\r?$",
                                                     trace[start:end], re.MULTILINE)]


def boot(args, disk, out, phase, seed):
    info = disk.lstat()
    require(disk.parent == out and stat.S_ISREG(info.st_mode) and info.st_nlink == 1,
            "QEMU requires a private regular disk image")
    variables = out / f"vars-{phase}.fd"
    shutil.copyfile(args.ovmf_vars, variables)
    trace = out / f"ata-{phase}.log"
    with tempfile.TemporaryDirectory(prefix="srv-ahci-", dir="/tmp") as tmp:
        qmp_path = Path(tmp) / "qmp"
        port = choose_port(30001, 38000)
        command = [args.qemu, "-machine", "q35,accel=tcg", "-m", "1024M", "-smp", "1", "-nic", "none",
                   "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf}",
                   "-drive", f"if=pflash,format=raw,file={variables}",
                   "-device", "ich9-ahci,id=ahci",
                   "-drive", f"if=none,id=testssd,format=raw,file={disk},cache=writeback,rerror=report,werror=report",
                   "-device", "ide-hd,drive=testssd,bus=ahci.0,bootindex=1",
                   "-serial", f"tcp:127.0.0.1:{port},server=on,wait=off",
                   "-qmp", f"unix:{qmp_path},server=on,wait=off",
                   "-trace", f"enable=ide_bus_exec_cmd,file={trace}",
                   "-display", "none", "-monitor", "none", "-no-reboot"]
        save_json(out / f"command-{phase}.json", command)
        output = bytearray()
        consumed = 0
        serial = qmp = None
        with (out / f"qemu-{phase}.log").open("wb") as log:
            vm = subprocess.Popen(command, stdout=log, stderr=log)
            try:
                serial = connect_serial(port, 20)
                serial.settimeout(0.2)
                qmp = QMP(qmp_path)

                def wait(marker, timeout):
                    nonlocal consumed
                    deadline = time.monotonic() + timeout
                    while marker not in output[consumed:]:
                        require(time.monotonic() < deadline and vm.poll() is None,
                                f"{phase}: timed out waiting for {marker[:80]!r}")
                        try:
                            chunk = serial.recv(16384)
                            require(chunk, f"{phase}: serial disconnected")
                            output.extend(chunk)
                        except socket.timeout:
                            pass
                    consumed = output.index(marker, consumed) + len(marker)

                def send(line):
                    serial.sendall(line.encode("ascii") + b"\n")

                def run(line):
                    start = len(output)
                    send("run " + line)
                    wait(b" exited status=", 45)
                    wait(b"\nsrv> ", 30)
                    require(re.search(rb"run: pid=\d+ exited status=0\r?\n", output[start:]),
                            f"{phase}: guest app failed: {line}")

                wait(b"\nsrv> ", 90)
                require(b"ahci: registered ahci0" in output and b"exfat: mounted /fat from ahci0p2" in output,
                        f"{phase}: did not mount the intended AHCI partition")
                before = block_stats(qmp)
                initialized = trace.stat().st_size
                during = None
                if phase == "write":
                    run("/fat/bin/dd if=/fat/data/source.bin of=/fat/data/persist.bin bs=4096 count=1")
                    during = block_stats(qmp)
                    saved = trace.stat().st_size
                    run("/fat/bin/sync")
                else:
                    send(f"cat /fat/{DESTINATION}")
                    wait(seed, 30)
                    wait(b"\nsrv> ", 30)
                after = block_stats(qmp)
                completed = trace.stat().st_size
                if phase == "write":
                    require(during["wr_bytes"] - before["wr_bytes"] >= len(seed), "Did not observe 4 KiB of disk writes")
                    require(after["flush_operations"] > during["flush_operations"],
                            "Explicit sync did not complete a QEMU SSD flush")
                else:
                    require(after["rd_bytes"] - before["rd_bytes"] >= len(seed),
                            "Fresh-boot readback did not reach the QEMU SSD")
                require(b"exception:" not in output, f"{phase}: guest exception")

                send("fsck /fat")
                wait(b"exfat-check:", 30)
                wait(b"\nsrv> ", 30)
                require(re.search(CLEAN, output), f"{phase}: guest allocation check failed")

                before_reboot = trace.stat().st_size
                send("reboot")
                qmp.sock.settimeout(30)
                while True:
                    line = qmp.stream.readline()
                    require(line, f"{phase}: QMP closed before guest reset")
                    event = json.loads(line)
                    if event.get("event") == "SHUTDOWN":
                        require(event.get("data", {}).get("guest") is True and
                                event["data"].get("reason") == "guest-reset",
                                f"{phase}: QEMU did not exit by guest reboot")
                        break
                require(vm.wait(timeout=30) == 0, f"{phase}: QEMU exited with error")
            finally:
                if serial:
                    serial.close()
                if qmp:
                    qmp.stream.close()
                    qmp.sock.close()
                stop_process(vm)
                (out / f"serial-{phase}.log").write_bytes(output)

    ata = trace.read_bytes()
    reboot_flushes = sum(op in (0xe7, 0xea) for op in ata_commands(ata, before_reboot))
    require(reboot_flushes > 0, f"{phase}: no ATA cache flush during guest reboot")
    result = dict(stats_before=before, stats_after=after, guest_reset=event["data"],
                  ata_reboot_flush_commands=reboot_flushes)
    if phase == "write":
        writes = ata_commands(ata, initialized, saved).count(0x35)
        flushes = sum(op in (0xe7, 0xea) for op in ata_commands(ata, saved, completed))
        require(writes >= 8 and flushes > 0, "Expected ATA write commands and an explicit sync flush")
        result.update(stats_after_write=during, ata_write_dma_ext_commands=writes, ata_sync_flush_commands=flushes)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--ovmf", default=default_ovmf_path(), type=Path)
    parser.add_argument("--ovmf-vars", required=True, type=Path)
    args = parser.parse_args()
    for path in (args.ovmf, args.ovmf_vars):
        require(path.is_file() and "," not in str(path), "Expected a regular OVMF firmware template")
    base = ROOT / "build/review/ahci-file-persistence"
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="proof-", dir=base)).resolve()
    print(f"File-backed SSD evidence: {out}", flush=True)
    receipt = dict(status="INCOMPLETE", source=source_provenance.capture(ROOT), physical_disk=False,
                   qemu=subprocess.check_output((args.qemu, "--version"), text=True).splitlines()[0],
                   firmware_sha256=digest(args.ovmf), vars_template_sha256=digest(args.ovmf_vars),
                   boots={})
    save_json(out / "result.json", receipt)
    try:
        disk, seed, receipt["inputs"] = make_disk(out)
        target = disk.stat()
        _, (offset, size) = partitions(disk)
        protected = outside_data(disk, offset, size)
        before = ExfatFixture(data_image(disk, offset, size))
        require(not any(name == "persist.bin" for name, _ in before.entries(before.read("data"))),
                "Destination already exists in the fixture")
        for phase in ("write", "readback"):
            receipt["boots"][phase] = boot(args, disk, out, phase, seed)
            current = disk.stat()
            require((current.st_dev, current.st_ino, current.st_size) ==
                    (target.st_dev, target.st_ino, target.st_size), "Disk image identity or size changed")
            extracted = out / f"{phase}.exfat"
            extracted.write_bytes(data_image(disk, offset, size))
            fixture = ExfatFixture(extracted.read_bytes())
            require(fixture.read(DESTINATION) == fixture.read("data/source.bin") == seed,
                    f"{phase}: exact source or destination bytes differ")
            require(outside_data(disk, offset, size) == protected, f"{phase}: write outside data partition")
            (out / f"fsck-{phase}.log").write_text(check_clean_image(extracted))
            save_json(out / "result.json", receipt)
        require(source_provenance.capture(ROOT) == receipt["source"], "Source changed during the trial")
        require(all(digest(ROOT / path) == recorded for path, recorded in receipt["inputs"].items()),
                "Boot or userspace input changed during the trial")
        receipt.update(status="PASS", payload_bytes=len(seed), payload_sha256=hashlib.sha256(seed).hexdigest(),
                       protected_regions=protected, data_partition=dict(offset=offset, bytes=size))
    except Exception as exc:
        receipt.update(status="FAIL", error=str(exc))
        raise
    finally:
        save_json(out / "result.json", receipt)
    print(f"File-backed SSD persistence: PASS ({out})", flush=True)


if __name__ == "__main__":
    main()
