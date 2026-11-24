#!/usr/bin/env python3
"""Boot the exact attended EHCI release once, then verify the GRUB default."""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import tempfile

import hardware_boot as boot
from ehci_hub_fixture import HubFixture
from hardware_boot_smoke import make_disk, wait_for
from uefi_usb_smoke import default_ovmf_path, has_fatal_exception, qmp_command


def boot_once(command, logs, label, ready, screenshot=None):
    serial = logs / f"{label}-serial.log"
    serial.unlink(missing_ok=True)
    with (logs / f"{label}-qemu.log").open("wb") as errors:
        with subprocess.Popen(command + ["-serial", f"file:{serial}"], stdout=errors, stderr=errors) as vm:
            try:
                text = wait_for(serial, ready, vm, 90)
                if screenshot is not None:
                    qmp_path, image = screenshot
                    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as qmp:
                        qmp.settimeout(5)
                        qmp.connect(str(qmp_path))
                        qmp.recv(4096)
                        qmp_command(qmp, {"execute": "qmp_capabilities"})
                        reply = qmp_command(qmp, {"execute": "screendump", "arguments": {"filename": str(image)}})
                        assert b'"return"' in reply and image.stat().st_size > 1024, reply
                return text
            finally:
                if vm.poll() is None:
                    vm.terminate()
                    try:
                        vm.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        vm.kill()
                        vm.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("release", help="exact staged EHCI trial release")
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    args = parser.parse_args()
    if not boot.RELEASE_RE.fullmatch(args.release):
        parser.error("expected a 16-digit release identifier")
    release = args.release
    source = boot.STAGE / release
    manifest = boot.verify(source, release)
    if manifest["config_version"] != boot.EHCI_TRIAL_VERSION:
        parser.error("expected an attended EHCI split trial stage")
    logs = boot.ROOT / "build/review" / f"macbookpro8-1-ehci-trial-{release}"
    logs.mkdir(parents=True, exist_ok=True)
    result_file = logs / "vm-result.json"
    result_file.unlink(missing_ok=True)

    with tempfile.TemporaryDirectory(prefix="srvros-ehci-trial-") as scratch:
        temp = Path(scratch)
        root_tree = temp / "root"
        env = root_tree / "boot/grub/grubenv"
        env.parent.mkdir(parents=True)
        subprocess.run(["grub-editenv", str(env), "create"], check=True)
        subprocess.run(["grub-editenv", str(env), "set", "next_entry=srvros-current"], check=True)
        root_disk = temp / "ubuntu-root.img"
        with root_disk.open("wb") as stream:
            stream.truncate(32 * 1024 * 1024)
        subprocess.run(["mke2fs", "-q", "-F", "-t", "ext4", "-L", "SRVROS_RETURN_VM",
                        "-d", str(root_tree), str(root_disk)], check=True)

        config = temp / "grub.cfg"
        config.write_text("""serial --unit=0 --speed=115200
terminal_output console serial
set prefix=(memdisk)/boot/grub
search --no-floppy --label --set=ubuntu_root SRVROS_RETURN_VM
set env_block=($ubuntu_root)/boot/grub/grubenv
load_env -f "$env_block"
if [ "${next_entry}" ]; then
    set default="${next_entry}"
    set next_entry=
    if save_env -f "$env_block" next_entry; then
        echo SRVROS_VM_NEXT_ENTRY_CONSUMED
    else
        echo SRVROS_VM_NEXT_ENTRY_SAVE_FAILED
    fi
else
    set default="0"
fi
menuentry 'Ubuntu return fixture' --id ubuntu {
    echo SRVROS_VM_UBUNTU_DEFAULT_SELECTED
    halt
}
""" + boot.grub_config("5352-5652", release) + "set timeout=0\n")
        subprocess.run(["grub-script-check", str(config)], check=True)
        loader = temp / "BOOTX64.EFI"
        subprocess.run(["grub-mkstandalone", "-O", "x86_64-efi", "--locales=", "--fonts=",
                        "--modules=part_gpt fat ext2 chain search search_fs_uuid search_label normal serial terminal loadenv test echo halt",
                        "-o", str(loader), f"boot/grub/grub.cfg={config}"], check=True)
        esp = temp / "esp.img"
        make_disk(esp, loader, source, release)
        command = [args.qemu, "-M", "q35,usb=off,i8042=off", "-m", "2G", "-nic", "none",
                   "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf}",
                   "-drive", f"format=raw,file={esp}", "-drive", f"format=raw,file={root_disk}",
                   "-monitor", "none", "-display", "none", "-no-reboot"]

        qmp = temp / "qmp.sock"
        endpoint = temp / "hub.sock"
        with HubFixture(endpoint) as hub:
            trial_command = command + ["-qmp", f"unix:{qmp},server=on,wait=off",
                "-device", "ich9-usb-ehci1,id=ehci1", "-device", "ich9-usb-ehci2,id=ehci2",
                "-chardev", f"socket,id=hub,path={endpoint}",
                "-device", "usb-redir,bus=ehci1.0,port=1,chardev=hub"]
            first = boot_once(trial_command, logs, "trial",
                              lambda s: "Screen held for photo." in s, (qmp, logs / "vm-screen.ppm"))
        expected = [
            "SRVROS_VM_NEXT_ENTRY_CONSUMED",
            f"cmdline: srvros.storage=ram srvros.ehci=split-trial srvros.ehci-release={release}",
            "1. Wait for the result.", "2. Photograph the entire screen", "3. Hold the power button",
            "Preparing the RAM-only EHCI diagnostic...", "Scanning EHCI controllers",
            "Controllers found 2. Scans complete 2. DMA released 2.",
            "Topcase 05ac:0245 via hub 0424:2513 port 2, EP0 8: unverified",
            f"ehci-trial: release={release} result=INCOMPLETE",
        ]
        for marker in expected:
            assert marker in first, marker
        assert not has_fatal_exception(first)
        assert "SRVROS_VM_NEXT_ENTRY_SAVE_FAILED" not in first and "SRVROS_VM_UBUNTU_DEFAULT_SELECTED" not in first
        assert all(marker not in first for marker in ("srv> ", "xhci:", "workspace-log:", "ahci:"))
        assert hub.powered == hub.status_ports == {1, 2, 3} and hub.hub_descriptors == 1

        saved = temp / "after.grubenv"
        subprocess.run(["debugfs", "-R", f"dump /boot/grub/grubenv {saved}", str(root_disk)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        assert subprocess.check_output(["grub-editenv", str(saved), "list"], text=True).strip() == "next_entry="
        second = boot_once(command, logs, "return", lambda s: "SRVROS_VM_UBUNTU_DEFAULT_SELECTED" in s)
        assert "SRVROS_VM_NEXT_ENTRY_CONSUMED" not in second and "srvros: entering kernel" not in second

    result = dict(status="PASS", release=release, manifest_sha256=boot.digest(source / "manifest.json"),
                  exact_payload=True, guest_trial_result="INCOMPLETE", topcase="absent in VM",
                  on_screen_checklist="vm-screen.ppm", grub_next_entry="", ubuntu_default_fixture=True)
    result_file.write_text(json.dumps(result, indent=2) + "\n")
    print(f"ehci-trial-smoke: PASS ({release}); {result_file}")


if __name__ == "__main__":
    main()
