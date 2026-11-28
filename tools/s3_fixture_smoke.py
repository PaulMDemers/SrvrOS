#!/usr/bin/env python3
"""Prove a pre-driver x86 S3 firmware wake in an isolated Q35 guest."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

import hardware_boot as boot
from desktop_foundation_smoke import QMP
from hardware_boot_smoke import make_disk, wait_for
from uefi_usb_smoke import default_ovmf_path, has_fatal_exception


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--ovmf", type=Path, default=Path(default_ovmf_path()))
    args = parser.parse_args()
    build = boot.ROOT / "build"
    parent = build / "review"
    parent.mkdir(parents=True, exist_ok=True)
    logs = Path(tempfile.mkdtemp(prefix="q35-s3-", dir=parent))
    serial = logs / "serial.log"
    kernel = build / "kernel/srvros.elf"
    digest = hashlib.sha256(kernel.read_bytes()).hexdigest()
    release = digest[:16]
    print(f"s3-fixture: evidence directory {logs}", flush=True)

    with tempfile.TemporaryDirectory(prefix="q35-s3-") as work:
        work = Path(work)
        payload = work / "payload"
        loader_dir = payload / "loader"
        loader_dir.mkdir(parents=True)
        loader = loader_dir / "BOOTX64.EFI"
        shutil.copyfile(build / "limine-binary/BOOTX64.EFI", loader)
        shutil.copyfile(kernel, payload / "srvros.elf")
        (payload / "initramfs.tar").write_bytes(boot.EHCI_TRIAL_INITRAMFS)
        config = boot.limine_config(release, boot.CONFIG_VERSION).replace(
            "cmdline: srvros.storage=ram", "cmdline: srvros.storage=ram srvros.s3=q35-fixture")
        for directory in (payload, loader_dir):
            (directory / "limine.conf").write_text(config)
        disk = work / "esp.img"
        make_disk(disk, loader, payload, release, {"limine.conf": config.encode()})
        qmp = work / "qmp.sock"
        command = [args.qemu, "-machine", "q35,accel=tcg", "-global", "ICH9-LPC.disable_s3=0",
                   "-smp", "1", "-m", "2G", "-nic", "none", "-display", "none", "-no-reboot",
                   "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf}",
                   "-drive", f"format=raw,file={disk}", "-serial", f"file:{serial}",
                   "-qmp", f"unix:{qmp},server=on,wait=off", "-monitor", "none",
                   "-d", "cpu_reset", "-D", str(logs / "cpu-reset.log")]
        with (logs / "qemu.log").open("w") as diagnostics:
            vm = subprocess.Popen(command, stdout=diagnostics, stderr=diagnostics)
            try:
                monitor = QMP(qmp)
                try:
                    wait_for(serial, lambda log: "s3-fixture: " in log, vm, 45)
                    first = serial.read_text(errors="replace")
                    assert "s3-fixture: enter vector=" in first, first.splitlines()[-4:]
                    for _ in range(100):
                        if monitor.call("query-status")["status"] == "suspended":
                            break
                        time.sleep(0.1)
                    else:
                        raise RuntimeError("QEMU did not enter suspended state")
                    print("s3-fixture: QMP observed suspended; triggering wake", flush=True)
                    monitor.call("system_wakeup")
                    final = wait_for(serial, lambda log: "s3-fixture: resumed=" in log, vm, 30)
                    assert "s3-fixture: resumed=1 vector_restored=1 page_retired=1 result=PASS" in final
                    assert final.count("srvros: entering kernel") == 1 and not has_fatal_exception(final)
                    assert vm.wait(timeout=10) == 0
                finally:
                    monitor.stream.close()
                    monitor.sock.close()
            finally:
                if vm.poll() is None:
                    vm.kill()
                    vm.wait()
    receipt = {"result": "PASS", "kernel_sha256": digest, "firmware": str(args.ovmf),
               "suspend_observed": True, "resume_same_boot": True, "wake_page_retired": True}
    (logs / "result.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"s3-fixture: PASS, {logs / 'result.json'}")


if __name__ == "__main__":
    main()
