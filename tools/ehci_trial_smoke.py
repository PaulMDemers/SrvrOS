#!/usr/bin/env python3
"""Boot the exact attended EHCI release once, then verify the GRUB default."""
import argparse
import json
from pathlib import Path
import re
import socket
import subprocess
import tempfile
from threading import Thread
import time

import hardware_boot as boot
from ehci_hub_fixture import HubFixture
from hardware_boot_smoke import make_disk, wait_for
from uefi_usb_smoke import default_ovmf_path, has_fatal_exception, qmp_command


def capture_screen(qmp_path, image):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as qmp:
        qmp.settimeout(5)
        qmp.connect(str(qmp_path))
        qmp.recv(4096)
        qmp_command(qmp, {"execute": "qmp_capabilities"})
        reply = qmp_command(qmp, {"execute": "screendump", "arguments": {"filename": str(image)}})
        assert b'"return"' in reply and image.stat().st_size > 1024, reply


def boot_once(command, logs, label, ready, screenshot=None):
    serial = logs / f"{label}-serial.log"
    serial.unlink(missing_ok=True)
    with (logs / f"{label}-qemu.log").open("wb") as errors:
        with subprocess.Popen(command + ["-serial", f"file:{serial}"], stdout=errors, stderr=errors) as vm:
            try:
                text = wait_for(serial, ready, vm, 90)
                if screenshot is not None:
                    capture_screen(*screenshot)
                return text
            finally:
                if vm.poll() is None:
                    vm.terminate()
                    try:
                        vm.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        vm.kill()
                        vm.wait()


def exercise_monitor(command, temp, logs, release):
    serial = logs / "trial-serial.log"
    serial.unlink(missing_ok=True)
    terminal, qmp = temp / "terminal.sock", temp / "qmp.sock"
    command += ["-qmp", f"unix:{qmp},server=on,wait=off",
                "-chardev", f"socket,id=terminal,path={terminal},server=on,wait=off,logfile={serial}",
                "-serial", "chardev:terminal"]
    with (logs / "trial-qemu.log").open("wb") as errors:
        with subprocess.Popen(command, stdout=errors, stderr=errors) as vm:
            try:
                first = wait_for(serial, lambda s: "srv> " in s, vm, 90)
                for marker in ("SRVROS_VM_NEXT_ENTRY_CONSUMED",
                               f"cmdline: srvros.storage=ram srvros.ehci=keyboard-monitor srvros.ehci-release={release}",
                               "With the built-in keyboard", "Press Up to recall it",
                               "ehci-monitor: captured controller absent, serial input available",
                               "srvros monitor ready."):
                    assert marker in first, marker
                assert all(marker not in first for marker in ("xhci:", "workspace-log:", "ahci:", "SRVROS_VM_NEXT_ENTRY_SAVE_FAILED"))
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as keys:
                    keys.connect(str(terminal))
                    # Drain serial output so repeated line redraws cannot stall the UART.
                    def drain():
                        while keys.recv(4096):
                            pass

                    reader = Thread(target=drain, daemon=True)
                    reader.start()
                    for key in b"echo p P q Q g G y Y\n":
                        keys.sendall(bytes([key]))
                        time.sleep(0.03)
                    wait_for(serial, lambda s: "\np P q Q g G y Y\nsrv> " in s, vm, 10)
                    for key in (b"\x1b[A", b"\x1b[D", b"\x7f", b"\n"):
                        keys.sendall(key)
                        time.sleep(0.05)
                    wait_for(serial, lambda s: "\np P q Q g G yY\nsrv> " in s, vm, 10)
                    capture_screen(qmp, logs / "vm-screen.ppm")
                    keys.sendall(b"reboot\n")
                    assert vm.wait(timeout=15) == 0
                    reader.join(timeout=5)
                final = serial.read_text(errors="replace")
                assert "reboot: ACPI reset port=" in final and not has_fatal_exception(final)
            finally:
                if vm.poll() is None:
                    vm.kill()
                    vm.wait()


def exercise_trackpad(command, temp, logs, release, log_volume):
    qmp = temp / "qmp.sock"
    command = command + ["-qmp", f"unix:{qmp},server=on,wait=off", "-device", "ich9-usb-ehci1"]
    first = boot_once(command, logs, "trial", lambda s: "Screen held for photo." in s,
                      (qmp, logs / "vm-screen.ppm"))
    for marker in ("SRVROS_VM_NEXT_ENTRY_CONSUMED",
                   f"cmdline: srvros.storage=ram srvros.ehci=trackpad-trial srvros.ehci-release={release}",
                   "move one finger across the pad", "place two fingers on it",
                   "Captured topcase controller absent.",
                   "Trackpad unavailable. Reports 0. Last 0 bytes. Largest 0 bytes.",
                   f"ehci-trackpad: release={release} result=INCOMPLETE"):
        assert marker in first, marker
    assert not has_fatal_exception(first)
    if log_volume:
        for marker in (f"srvros.ehci-log-volume={log_volume}", "Keep the flash drive connected.",
                       "ehci-log: captured flash controller absent", "USB result save could not be verified."):
            assert marker in first, marker
    for marker in ("Trackpad capture starting. Start the checklist now.", "Trackpad HID replay completed=",
                   "Trackpad mode-to-arm request",
                   "Trackpad first payload offset", "ehci: bus=", "srv> ", "xhci:",
                   "workspace-log:", "ahci:", "SRVROS_VM_NEXT_ENTRY_SAVE_FAILED"):
        assert marker not in first, marker


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("release", help="exact staged EHCI trial release")
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    parser.add_argument("--stall-hub-status", action="store_true", help="inject a port 2 GET_STATUS stall")
    args = parser.parse_args()
    if not boot.RELEASE_RE.fullmatch(args.release):
        parser.error("expected a 16-digit release identifier")
    release = args.release
    source = boot.STAGE / release
    manifest = boot.verify(source, release)
    version = manifest["config_version"]
    log_match = boot.EHCI_TRACKPAD_LOG_RE.fullmatch(version)
    log_volume = log_match[1] if log_match else None
    if version not in boot.EHCI_TRIAL_VERSIONS and not log_volume:
        parser.error("expected an attended EHCI trial stage")
    keyboard_trial = version == boot.EHCI_KEYBOARD_VERSION
    keyboard_monitor = version == boot.EHCI_MONITOR_VERSION
    trackpad_trial = version == boot.EHCI_TRACKPAD_VERSION or bool(log_volume)
    if (keyboard_monitor or trackpad_trial) and args.stall_hub_status:
        parser.error("the emulated hub is for the split and typing trials")
    mode = "trackpad-trial" if log_volume else boot.EHCI_TRIAL_MODES[version]
    logs = boot.ROOT / "build/review" / f"macbookpro8-1-ehci-trial-{release}"
    if args.stall_hub_status:
        logs /= "hub-status-stall"
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

        if keyboard_monitor:
            exercise_monitor(command.copy(), temp, logs, release)
        elif trackpad_trial:
            exercise_trackpad(command, temp, logs, release, log_volume)
        else:
            scans = exercise_trial(command, temp, logs, release, mode, keyboard_trial, args.stall_hub_status)

        saved = temp / "after.grubenv"
        subprocess.run(["debugfs", "-R", f"dump /boot/grub/grubenv {saved}", str(root_disk)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        assert subprocess.check_output(["grub-editenv", str(saved), "list"], text=True).strip() == "next_entry="
        second = boot_once(command, logs, "return", lambda s: "SRVROS_VM_UBUNTU_DEFAULT_SELECTED" in s)
        assert "SRVROS_VM_NEXT_ENTRY_CONSUMED" not in second and "srvros: entering kernel" not in second

    result = dict(status="PASS", release=release, manifest_sha256=boot.digest(source / "manifest.json"),
                  exact_payload=True, topcase="absent in VM",
                  on_screen_checklist="vm-screen.ppm", grub_next_entry="", ubuntu_default_fixture=True)
    if keyboard_monitor:
        result.update(monitor_input="serial", case_echo=True, history_edit=True, monitor_reboot=True)
    elif trackpad_trial:
        result.update(guest_trial_result="INCOMPLETE", unrelated_controller_skipped=True, trackpad_reports=0)
    else:
        result.update(guest_trial_result="INCOMPLETE", keyboard_trial=keyboard_trial,
                      scans_complete=scans, hub_status_stall=args.stall_hub_status)
    result_file.write_text(json.dumps(result, indent=2) + "\n")
    print(f"ehci-trial-smoke: PASS ({release}); {result_file}")


def exercise_trial(command, temp, logs, release, mode, keyboard_trial, stall_hub_status):
    qmp = temp / "qmp.sock"
    endpoint = temp / "hub.sock"
    with HubFixture(endpoint, stall_status_port=2 if stall_hub_status else None) as hub:
        trial_command = command + ["-qmp", f"unix:{qmp},server=on,wait=off",
            "-device", "ich9-usb-ehci1,id=ehci1", "-device", "ich9-usb-ehci2,id=ehci2",
            "-chardev", f"socket,id=hub,path={endpoint}",
            "-device", "usb-redir,bus=ehci1.0,port=1,chardev=hub"]
        first = boot_once(trial_command, logs, "trial",
                          lambda s: "Screen held for photo." in s, (qmp, logs / "vm-screen.ppm"))
    scans = 1 if stall_hub_status else 2
    expected = [
        "SRVROS_VM_NEXT_ENTRY_CONSUMED",
        f"cmdline: srvros.storage=ram srvros.ehci={mode} srvros.ehci-release={release}",
        "2. Photograph the entire screen", "3. Hold the power button",
        "Preparing the RAM-only EHCI diagnostic...", "Scanning EHCI controllers",
        f"Controllers found 2. Scans complete {scans}. DMA released 2.",
        "Topcase 05ac:0245 via hub 0424:2513 port 2, EP0 8: unverified",
        "Flash 13fe:6700 USB address 0 (0 means unverified).",
        f"ehci-trial: release={release} result=INCOMPLETE",
    ]
    if keyboard_trial:
        expected += ["1. At srv>, type echo Ehci-42", "Keyboard unavailable. Reports 0. Last bytes 0."]
        assert "Keyboard idle 32 ms" not in first and "FRINDEX " not in first
    else:
        expected.append("1. Wait for the result.")
    if stall_hub_status:
        expected += ["Stop at hub 1 port 2 during hub port preflight", "last port status unavailable",
                     "Control qTD error at USB address 1, request 0",
                     "ehci: control failure=qTD error address=1 type=0x00000000000000a3 request=0x0000000000000000 "
                     "value=0x0000000000000000 index=2 length=4"]
    for marker in expected:
        assert marker in first, marker
    assert not has_fatal_exception(first)
    assert "SRVROS_VM_NEXT_ENTRY_SAVE_FAILED" not in first and "SRVROS_VM_UBUNTU_DEFAULT_SELECTED" not in first
    assert all(marker not in first for marker in ("srv> ", "xhci:", "workspace-log:", "ahci:"))
    assert "Flash EP0 wait" not in first
    assert hub.powered == {1, 2, 3} and hub.hub_descriptors == 1
    if stall_hub_status:
        assert hub.status_ports == {1, 2} and hub.stalled_status_requests == 1
        assert re.search(r"(?m)^  USBSTS=0x[0-9a-f]{16}  last port status unavailable$", first)
        assert re.search(r"(?m)^  qTD status bytes \(decimal\) setup=\d+ data=\d+ status=\d+$", first)
    else:
        assert hub.status_ports == {1, 2, 3} and not hub.stalled_status_requests

    return scans


if __name__ == "__main__":
    main()
