#!/usr/bin/env python3
"""Exercise the staged GRUB -> Limine -> kernel path on a disposable QEMU ESP."""
import argparse
import binascii
import re
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import uuid

import hardware_boot as boot
from mk_usb_image import Fat32Builder, ESP_TYPE, gpt_entry, gpt_header
from uefi_usb_smoke import default_ovmf_path, qmp_command, send_qmp_text, send_qmp_key, has_fatal_exception


def make_disk(path, loader, source, release, extra_files=None):
    # Same paths and filesystem UUID search as the host entry; only the UUID differs.
    size = 384 * 1024 * 1024
    first, sectors = 2048, size // 512
    last = first + sectors - 1
    backup = last + 2048
    fat = Fat32Builder(size, first)
    efi = fat.add_dir(fat.root_cluster, "EFI")
    fallback = fat.add_dir(efi, "BOOT")
    fat.add_file(fallback, "BOOTX64.EFI", loader.read_bytes())
    srvros = fat.add_dir(efi, "srvros")
    payload = fat.add_dir(srvros, release)
    loader_dir = fat.add_dir(payload, "loader")
    for name in boot.FILES:
        parent = loader_dir if name.startswith("loader/") else payload
        fat.add_file(parent, Path(name).name, (source / name).read_bytes())
    for name, data in (extra_files or {}).items():
        fat.add_file(fat.root_cluster, name, data)
    entries = bytearray(128 * 128)
    entries[:128] = gpt_entry(ESP_TYPE, uuid.uuid4(), first, last, "SrvrOS test ESP")
    crc, guid = binascii.crc32(entries) & 0xffffffff, uuid.uuid4()
    mbr = bytearray(512)
    mbr[446:462] = struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0xee, b"\xff\xff\xff", 1, backup)
    mbr[510:512] = b"\x55\xaa"
    with path.open("wb") as stream:
        stream.truncate((backup + 1) * 512)
        stream.write(mbr)
        stream.write(gpt_header(1, backup, 34, backup - 33, 2, 128, crc, guid))
        stream.write(entries)
        stream.seek(first * 512)
        stream.write(fat.build())
        stream.seek((backup - 32) * 512)
        stream.write(entries)
        stream.seek(backup * 512)
        stream.write(gpt_header(backup, 1, 34, backup - 33, backup - 32, 128, crc, guid))


def wait_for(log, predicate, process, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        text = log.read_text(errors="replace") if log.exists() else ""
        if predicate(text):
            return text
        if process.poll() is not None:
            raise RuntimeError(f"QEMU exited with {process.returncode}; see {log.parent}")
        time.sleep(0.2)
    raise RuntimeError(f"Timed out; see {log}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot-wait", type=float, default=180)
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--desktop-check", action="store_true")
    parser.add_argument("--console-check", action="store_true")
    parser.add_argument("--kernel-check", action="store_true")
    parser.add_argument("--responsiveness-check", action="store_true")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    args = parser.parse_args()
    release = (boot.STAGE / "latest").read_text().strip()
    source = boot.STAGE / release
    manifest=boot.verify(source)
    desktop=boot.DESKTOP_RE.fullmatch(manifest.get("config_version", ""))
    if bool(desktop) != args.desktop_check:
        parser.error("Native desktop stages require --desktop-check; other stages cannot use it")
    logs = boot.ROOT / "build/review"
    logs.mkdir(parents=True, exist_ok=True)
    serial = logs / "hardware-grub-serial.log"
    serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="srvros-grub-") as tmp:
        temp = Path(tmp)
        cfg = temp / "grub.cfg"
        cfg.write_text("serial --unit=0 --speed=115200\nterminal_output console serial\n"
                       "echo SRVROS_GRUB_CHAINLOAD_TEST\nset default=srvros-current\n" +
                       boot.grub_config("5352-5652", release))
        subprocess.run(["grub-script-check", str(cfg)], check=True)
        loader = temp / "BOOTX64.EFI"
        subprocess.run(["grub-mkstandalone", "-O", "x86_64-efi", "--locales=", "--fonts=",
                        "--modules=part_gpt fat chain search search_fs_uuid normal serial terminal",
                        "-o", str(loader), f"boot/grub/grub.cfg={cfg}"], check=True)
        disk = temp / "esp.img"
        make_disk(disk, loader, source, release)
        qmp_path = temp / "qmp.sock"
        command = [args.qemu, "-M", "q35", "-m", "2G",
                   "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf}",
                   "-drive", f"format=raw,file={disk}",
                   "-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0",
                   "-serial", f"file:{serial}", "-qmp", f"unix:{qmp_path},server=on,wait=off",
                   "-monitor", "none", "-display", "none", "-no-reboot"]
        if args.desktop_check:
            from exfat_test_image import ExfatFixture, check_clean_image
            volume=temp/"workspace.exfat";stick=temp/"workspace.img"
            with volume.open("wb") as stream: stream.truncate(128*1024*1024)
            subprocess.run(["/usr/sbin/mkfs.exfat","-c","128K",str(volume)],check=True,stdout=subprocess.DEVNULL)
            subprocess.run(["/usr/sbin/tune.exfat","-I","0x"+desktop[1],str(volume)],check=True,stdout=subprocess.DEVNULL)
            fixture=ExfatFixture(volume.read_bytes())
            fixture.add_directory_file("srvros-work",".workspace",f"SRVROS-WORKSPACE-V1 {desktop[1]}\n".encode())
            volume.write_bytes(fixture.data);check_clean_image(volume)
            mbr=bytearray(512);mbr[450]=7
            struct.pack_into("<II",mbr,454,2048,len(fixture.data)//512);mbr[510:]=b"\x55\xaa"
            with stick.open("wb") as stream: stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
            command += ["-drive",f"if=none,id=stick,format=raw,file={stick}","-device","usb-storage,drive=stick,bus=xhci.0"]
        with (logs / "hardware-grub-qemu.log").open("wb") as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                if args.desktop_check:
                    from desktop_foundation_smoke import QMP
                    wait_for(serial,lambda s:"deskhelp: page=laptop-trial" in s and "deskd: opened" in s,process,args.boot_wait)
                    ui=QMP(qmp_path)
                    def shortcut(keys):
                        ui.call("human-monitor-command",{"command-line":"sendkey "+keys});time.sleep(.3)
                    def type_text(value):
                        keys={" ":"spc","/":"slash",".":"dot","-":"minus"}
                        for c in value: ui.key(keys.get(c,c.lower()),c.isupper())
                    time.sleep(.5)
                    ui.call("screendump",{"filename":str(logs/"native-hardware-checklist.ppm")})
                    shortcut("meta_l-spc");type_text("Text new app");ui.key("ret")
                    wait_for(serial,lambda s:"window=1" in s,process,30)
                    time.sleep(.7);type_text("Laptop trial AaZz09 saved on USB.")
                    shortcut("ctrl-l");type_text("/work/srvros-work/native-trial.txt")
                    shortcut("ctrl-s")
                    wait_for(serial,lambda s:"picker ready mode=2" in s,process,30);ui.key("ret")
                    wait_for(serial,lambda s:"file-result role=text op=2 status=0" in s,process,60)
                    shortcut("f10");shortcut("end");shortcut("ret")
                    wait_for(serial,lambda s:"desktop-trial: desktop returned=yes USB unmounted=yes" in s and s.endswith("srv> "),process,60)
                    with stick.open("rb") as stream: stream.seek(2048*512);saved=stream.read()
                    volume.write_bytes(saved);check_clean_image(volume)
                    assert ExfatFixture(saved).read("srvros-work/native-trial.txt")==b"Laptop trial AaZz09 saved on USB."
                    ui.stream.close();ui.sock.close()
                text = wait_for(serial, lambda s: s.endswith("srv> "), process, args.boot_wait)
                for marker in ("SRVROS_GRUB_CHAINLOAD_TEST", "srvros: entering kernel",
                               "lpss-spi: disabled (experimental transport)",
                               "storage: RAM-only mode; AHCI disabled",
                               "exfat: mounted /fat from initramfs-exfat"):
                    if marker not in text:
                        raise RuntimeError(f"Missing boot marker: {marker}")
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as qmp:
                    qmp.settimeout(5)
                    qmp.connect(str(qmp_path))
                    qmp.recv(4096)
                    qmp_command(qmp, {"execute": "qmp_capabilities"})
                    send_qmp_text(qmp, "echo grub-usb-ok\n", 0.08)
                    text = wait_for(serial, lambda s: "\ngrub-usb-ok\n" in s, process, 20)
                    if args.kernel_check:
                        send_qmp_text(qmp, "fpustress\n", 0.08)
                        text = wait_for(serial, lambda s: "fpustress: PASS" in s and s.endswith("srv> "), process, 30)
                        probe = re.search(r"fpustress: PASS iterations=128 traps=256 failures=0 ticks=(\d+) preemptions=(\d+)", text)
                        if probe is None or int(probe[1]) < 128 or int(probe[2]) == 0:
                            raise RuntimeError("Kernel trap probe lacked timer/preemption coverage")
                        send_qmp_text(qmp, "fsck /fat\n", 0.08)
                        text = wait_for(serial, lambda s: "errors=0 ok" in s and s.endswith("srv> "), process, 60)
                        send_qmp_text(qmp, "block\n", 0.08)
                        text = wait_for(serial, lambda s: "block: cache entries=" in s and s.endswith("srv> "), process, 20)
                        cache = re.search(r"block: cache entries=.* service_windows=(\d+)", text)
                        if cache is None or int(cache[1]) == 0:
                            raise RuntimeError("RAM filesystem did not offer cache service windows")
                    if args.console_check:
                        if "calibrated=yes target=100Hz" not in text or "init: readiness=ready" not in text:
                            raise RuntimeError("Timer calibration or init readiness missing")
                        send_qmp_text(qmp, "echo aAzZ []{}~!\n", 0.08)
                        text = wait_for(serial, lambda s: "\naAzZ []{}~!\n" in s, process, 20)
                        send_qmp_text(qmp, "echo ", 0.08)
                        send_qmp_key(qmp, "caps_lock")
                        time.sleep(0.1)
                        send_qmp_text(qmp, "aA", 0.08)
                        send_qmp_key(qmp, "caps_lock")
                        time.sleep(0.1)
                        send_qmp_text(qmp, "\n", 0.08)
                        text = wait_for(serial, lambda s: "\nAa\n" in s, process, 20)
                        send_qmp_text(qmp, "echo ", 0.08)
                        for down in (True, False):
                            qmp_command(qmp, {"execute": "input-send-event", "arguments": {
                                "events": [{"type": "key", "data": {"down": down,
                                    "key": {"type": "qcode", "data": "a"}}}]}})
                            time.sleep(0.7 if down else 0.1)
                        send_qmp_text(qmp, "\n", 0.08)
                        text = wait_for(serial, lambda s: re.search(r"\na{2,}\n", s) is not None,
                                        process, 20)
                        send_qmp_text(qmp, "renderbench\n", 0.08)
                        text = wait_for(serial, lambda s: "serial=off complete=yes" in s and
                                        s.endswith("srv> "), process, 45)
                        time.sleep(0.2)  # Allow the deferred prompt to reach the framebuffer.
                        qmp_command(qmp, {"execute": "screendump", "arguments": {
                            "filename": str(logs / "console-performance.ppm")}})

                    if args.responsiveness_check:
                        send_qmp_text(qmp, "latency reset\n", 0.08)
                        wait_for(serial, lambda s: "latency: reset active=yes" in s and s.endswith("srv> "), process, 20)
                        send_qmp_text(qmp, "renderstress\n", 0.08)
                        wait_for(serial, lambda s: "renderstress: running" in s, process, 20)
                        send_qmp_text(qmp, "aAzZ123", 0.08)
                        text = wait_for(serial, lambda s: "renderstress: input=" in s and s.endswith("srv> "), process, 30)
                        if "renderstress: input=aAzZ123\n" not in text:
                            raise RuntimeError("USB input lost or changed during output stress")
                        result = re.search(r"renderstress: lines=(\d+) us=(\d+) ticks=(\d+) keys=7 dropped=0 complete=yes stopped=no", text)
                        if result is None or int(result[1]) < 32 or int(result[3]) < 2:
                            raise RuntimeError("Sustained output did not complete with timer/input progress")
                        send_qmp_text(qmp, "latency off\n", 0.08)
                        text = wait_for(serial, lambda s: "latency: usb-poll-gap" in s and s.endswith("srv> "), process, 20)
                        for name in ("console", "timer-gap", "usb-poll-gap"):
                            sample = re.search(r"latency: " + name + r" count=(\d+) max_us=(\d+)", text)
                            if sample is None or int(sample[1]) == 0:
                                raise RuntimeError(f"Missing latency samples: {name}")
                        time.sleep(0.2)
                        qmp_command(qmp, {"execute": "screendump", "arguments": {
                            "filename": str(logs / "console-latency.ppm")}})

                        # Escape must cancel a second run and leave the monitor usable.
                        offset = len(text)
                        send_qmp_text(qmp, "renderstress\n", 0.08)
                        wait_for(serial, lambda s: "renderstress: running" in s[offset:], process, 20)
                        send_qmp_key(qmp, "esc")
                        text = wait_for(serial, lambda s: "stopped=yes" in s[offset:] and s.endswith("srv> "), process, 20)

                        # Exercise the user syscall output path and USB Ctrl-C signal
                        # delivery while serial mirroring and framebuffer output run.
                        send_qmp_text(qmp, "run /fat/bin/sh\n", 0.08)
                        wait_for(serial, lambda s: s.endswith(" $ "), process, 20)
                        send_qmp_text(qmp, "yes latency-output\n", 0.08)
                        wait_for(serial, lambda s: s.count("\nlatency-output\n") >= 10, process, 20)
                        for down in (True, False):
                            qmp_command(qmp, {"execute": "input-send-event", "arguments": {"events": [
                                {"type": "key", "data": {"down": down, "key": {"type": "qcode", "data": key}}}
                                for key in ("ctrl", "c")]}})
                            time.sleep(0.1)
                        wait_for(serial, lambda s: s.endswith(" $ "), process, 20)
                        send_qmp_text(qmp, "echo survived-output\n", 0.08)
                        wait_for(serial, lambda s: "\nsurvived-output\n" in s and s.endswith(" $ "), process, 20)
                        send_qmp_text(qmp, "exit\n", 0.08)
                        text = wait_for(serial, lambda s: s.endswith("srv> "), process, 20)

                if has_fatal_exception(text):
                    raise RuntimeError("Fatal kernel exception in chainload test")
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    print(f"hardware-boot-smoke: PASS ({release}); GRUB chainload, RAM filesystem, USB input" + (", case/Caps/repeat/renderbench" if args.console_check else ""))
    if args.kernel_check:
        print("kernel: PASS (nested register preservation, timer/preemption, RAM fsck, cache service windows)")
    if args.responsiveness_check:
        print("responsiveness: PASS (USB input under output load, timer progress, latency samples, Escape, userspace Ctrl-C)")
    if args.desktop_check:
        print("Native desktop trial: checklist, USB keyboard editing, verified GUI save and clean unmount passed.")
    print(f"Serial log: {serial}")


if __name__ == "__main__":
    main()
