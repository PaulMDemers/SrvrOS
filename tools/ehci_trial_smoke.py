#!/usr/bin/env python3
"""Boot the exact EHCI trial release once, then verify the GRUB default."""
import argparse
import json
from pathlib import Path
import re
import socket
import struct
import subprocess
import tempfile
from threading import Thread
import time

import hardware_boot as boot
from desktop_foundation_smoke import QMP
from ehci_hub_fixture import HubFixture
from exfat_test_image import ExfatFixture, check_clean_image
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
                   "Repeat if a trackpad mode retry starts.", "Repeat when the mouse endpoint check starts.",
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
                   "Trackpad feature before payload=", "trackpad-qh endpoint=", "Trackpad running queue starts=",
                   "Trackpad mode-to-arm request",
                   "Trackpad first payload offset", "trackpad-usb endpoint=", "trackpad-retry trigger-completion=",
                   "ehci: bus=", "srv> ", "xhci:",
                   "workspace-log:", "ahci:", "SRVROS_VM_NEXT_ENTRY_SAVE_FAILED"):
        assert marker not in first, marker


def exercise_desktop(command, temp, logs, release, log_volume, mode, keyboard_checklist, keyboard_exit, file_checklist):
    saved_file = mode == "desktop-file"
    brightness = mode == "desktop-brightness"
    audio = mode == "desktop-audio"
    telemetry = mode == "desktop-telemetry"
    battery = mode == "desktop-battery"
    ethernet = mode == "desktop-ethernet"
    live_workspace = saved_file or brightness or audio or telemetry or battery or ethernet
    serial = logs / "trial-serial.log"
    serial.unlink(missing_ok=True)
    qmp = temp / "qmp.sock"
    command = [arg.replace("i8042=off", "i8042=on") for arg in command]
    command += ["-qmp", f"unix:{qmp},server=on,wait=off", "-serial", f"file:{serial}"]
    geometry = r"deskd: geometry pid=\d+ window=\d+ frame=(\d+),(\d+),(\d+),(\d+)"
    zoom = r"deskd: zoom pid=\d+ maximized=1 frame=(\d+),(\d+),(\d+),(\d+)"
    with (logs / "trial-qemu.log").open("wb") as errors:
        with subprocess.Popen(command, stdout=errors, stderr=errors) as vm:
            try:
                first = wait_for(serial, lambda s: re.search(geometry, s), vm, 90)
                for marker in ("SRVROS_VM_NEXT_ENTRY_CONSUMED", "ehci-desktop: captured trackpad controller absent",
                               f"srvros.ehci={mode} srvros.ehci-release={release} srvros.ehci-log-volume={log_volume}",
                               "deskd: display physical=1280x800 logical=1280x800 scale=1", "input: ps2_keyboard=yes ps2_mouse=yes"):
                    assert marker in first, marker
                if mode == "topcase-desktop" or live_workspace:
                    saved = "saved" if live_workspace else "unverified"
                    for marker in (f"ehci-desktop-startup: release={release} phase=startup",
                                   "ehci-desktop-queue: phase=startup endpoint=0x0000000000000081",
                                   "ehci-desktop-queue: phase=startup endpoint=0x0000000000000083",
                                   f"ehci-desktop-startup: USB checkpoint={saved}"):
                        assert first.index(marker) < first.index("ehci-desktop: opening desktop"), marker
                ui = QMP(qmp)
                if ethernet:
                    size = len(boot.EHCI_DESKTOP_ETHERNET_CHECKLIST)
                    wait_for(serial, lambda s: f"file-result role=text op=1 status=0 bytes={size}" in s, vm, 15)
                    ui.call("screendump", {"filename": str(logs / "vm-screen.ppm")})
                    final = wait_for(serial, lambda s: "USB report saved. Restarting into Ubuntu." in s, vm, 35)
                    for marker in ("bcm57765: refused platform identity", "ehci-desktop-ethernet: network_started=0",
                                   "ehci-desktop-ethernet-ui: provider unavailable",
                                   "ehci-desktop-ethernet-ui: result=INCOMPLETE phases=0",
                                   "ehci-desktop-ethernet: report=saved flash-retirement=safe"):
                        assert marker in final, marker
                    assert not has_fatal_exception(final)
                    ui.sock.close()
                    assert vm.wait(timeout=20) == 0
                    return
                if telemetry or battery:
                    wait_for(serial, lambda s: "desktools: system-page=sensors" in s, vm, 15)
                    time.sleep(1)
                    ui.call("screendump", {"filename": str(logs / "vm-screen.ppm")})
                    final = wait_for(serial, lambda s: "USB report saved. Restarting into Ubuntu." in s, vm, 40)
                    markers = ("pro81-telemetry: phase=startup cpu_state=1 cpu_fresh=0",
                                   "pro81-telemetry: phase=startup fan_state=1 fan_fresh=0",
                                   "ehci-desktop-telemetry-ui: result=PASS planned-exit=1",
                                   "pro81-telemetry: result=INCOMPLETE smc_quiesced=1",
                                   f"ehci-desktop: release={release} result=INCOMPLETE desktop=passed retirement=safe",
                                   "ehci-desktop-telemetry: report=saved flash-retirement=safe") if telemetry else (
                                   "pro81-battery: phase=startup state=5 fresh=0",
                                   "desktop-ac: provider-unavailable phase=0",
                                   "ehci-desktop-battery-ui: result=INCOMPLETE phases=0",
                                   "pro81-battery: result=INCOMPLETE smc_quiesced=1",
                                   f"ehci-desktop: release={release} result=INCOMPLETE desktop=unverified retirement=safe",
                                   "ehci-desktop-battery: report=saved flash-retirement=safe")
                    for marker in markers:
                        assert marker in final, marker
                    assert not has_fatal_exception(final)
                    ui.sock.close()
                    return
                time.sleep(1)
                ui.call("screendump", {"filename": str(logs / "vm-screen.ppm")})
                pointer = [24, 56]

                def click(x, y):
                    ui.mouse(x - pointer[0], y - pointer[1]); pointer[:] = [x, y]
                    ui.mouse(0, 0, 1)
                    ui.mouse(0, 0, 0)

                x, y, width, _ = map(int, re.search(geometry, first).groups())
                original_x, original_y = x, y
                if keyboard_checklist:
                    ui.call("human-monitor-command", {"command-line": "sendkey ctrl-f"})
                    time.sleep(.3)
                    ui.key("e", shift=True)
                    for key in ("h", "c", "i", "minus", "4", "2"):
                        ui.key(key)
                    wait_for(serial, lambda s: "ehci-desktop-ui: keyboard-checklist=matched" in s, vm, 10)
                    time.sleep(.4)
                    ui.call("screendump", {"filename": str(logs / "vm-typed-checklist.ppm")})
                    ui.key("esc")
                if keyboard_exit:
                    ui.call("human-monitor-command", {"command-line": "sendkey ctrl-alt-esc"})
                    ui_status = "INCOMPLETE axes=0 checklist-zoom-clicks=0 pointer-exit=0 timeout=0"
                    desktop_status = "unverified"
                else:
                    click(x + width - 56, y + 20)
                    resized = wait_for(serial, lambda s: re.search(zoom, s), vm, 15)
                    x, y, width, _ = map(int, re.search(zoom, resized).groups())
                    click(x + width - 56, y + 20)
                    wait_for(serial, lambda s: "maximized=0 frame=" in s, vm, 15)
                    if saved_file:
                        seed = len(file_checklist)
                        wait_for(serial, lambda s: f"file-result role=text op=1 status=0 bytes={seed}" in s, vm, 15)
                        if file_checklist == boot.EHCI_DESKTOP_FILE_V4_CHECKLIST:
                            ui.call("human-monitor-command", {"command-line": "sendkey ctrl-end"})
                        else:
                            click(original_x + 35, original_y + 448)
                        time.sleep(.3)
                        ui.key("e", shift=True)
                        for key in ("h", "c", "i", "minus", "4", "2"):
                            ui.key(key)
                        ui.call("human-monitor-command", {"command-line": "sendkey ctrl-s"})
                        wait_for(serial, lambda s: "file-result role=text op=2 status=0" in s, vm, 25)
                        ui.call("human-monitor-command", {"command-line": "sendkey ctrl-l"})
                        time.sleep(.3)
                        ui.key("ret")
                        wait_for(serial, lambda s: f"file-result role=text op=1 status=0 bytes={seed + 7}" in s, vm, 25)
                        time.sleep(.4)
                        ui.call("screendump", {"filename": str(logs / "vm-reopened-document.ppm")})
                    if brightness or audio:
                        click(790, 750)  # Settings in the 1280 × 800 dock.
                        wait_for(serial, lambda s: "desktools: brightness percent=-1" in s, vm, 20)
                    click(33, 20)  # SrvrOS menu.
                    click(89, 462)  # Return to console.
                    ui_status = "PASS axes=3 checklist-zoom-clicks=2 pointer-exit=1 timeout=0"
                    desktop_status = "passed"
                completion = "USB report saved. Restarting into Ubuntu." if live_workspace else "USB report save could not be verified."
                final = wait_for(serial, lambda s: completion in s, vm, 30)
                for marker in (f"ehci-desktop-ui: result={ui_status}",
                               f"ehci-desktop: release={release} result=INCOMPLETE desktop={desktop_status} retirement=safe"):
                    assert marker in final, marker
                if audio:
                    assert "pro81-audio: provider=unavailable controller identity mismatch" in first
                    assert "pro81-audio: phase=startup provider=unavailable volume=25" in first
                    assert "pro81-audio: phase=final result=INCOMPLETE started=0 completed=0 position=0 quiet=0 error=0" in final
                    assert "ehci-desktop-workspace: backend=usb mounted=1" in final
                    assert "ehci-desktop-audio: report=saved flash-retirement=safe" in final
                elif brightness:
                    assert "pro81-backlight: provider=unavailable identity mismatch" in first
                    assert "pro81-backlight: result=INCOMPLETE initial_duty=0 last_duty=0 writes=0 low=0 high=0 restored=0" in final
                    assert "ehci-desktop-workspace: backend=usb mounted=1" in final
                    assert "ehci-desktop-brightness: report=saved flash-retirement=safe" in final
                elif saved_file:
                    assert "ehci-desktop-workspace: backend=usb mounted=1" in final
                    assert f"ehci-desktop-file: release={release} path=/work/srvros-work/ehci-desktop-{release}.txt seeded=1 exact={int(not keyboard_exit)}" in final
                    assert "ehci-desktop-file: report=saved flash-retirement=safe" in final
                else:
                    assert "ehci-log: captured flash controller absent" in final
                if mode == "topcase-desktop":
                    assert "ehci-desktop-queue: phase=final endpoint=0x0000000000000081" in final
                    assert "ehci-desktop-queue: phase=final endpoint=0x0000000000000083" in final
                    assert final.count("ehci-log: captured flash controller absent") == 2
                if keyboard_exit:
                    assert "ehci-desktop-ui: keyboard-exit=1" in final
                assert not has_fatal_exception(final)
                ui.sock.close()
            finally:
                if vm.poll() is None:
                    vm.kill()
                    vm.wait()


def desktop_workspace_image(temp, serial):
    volume = temp / "workspace.exfat"
    with volume.open("wb") as stream:
        stream.truncate(128 * 1024 * 1024)
    subprocess.run(["/usr/sbin/mkfs.exfat", "-c", "128K", str(volume)], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["/usr/sbin/tune.exfat", "-I", "0x" + serial, str(volume)], check=True, stdout=subprocess.DEVNULL)
    image = ExfatFixture(volume.read_bytes())
    image.add_directory_files("srvros-work", {".workspace": f"SRVROS-WORKSPACE-V1 {serial}\n".encode(),
                                              "anchor.txt": b"workspace anchor\n"})
    mbr = bytearray(512)
    mbr[450] = 7
    struct.pack_into("<II", mbr, 454, 2048, len(image.data) // 512)
    mbr[510:] = b"\x55\xaa"
    stick = temp / "workspace.img"
    with stick.open("wb") as stream:
        stream.write(mbr)
        stream.seek(2048 * 512)
        stream.write(image.data)
    return stick, volume


def read_workspace(stick, volume):
    with stick.open("rb") as stream:
        stream.seek(2048 * 512)
        saved = stream.read()
    volume.write_bytes(saved)
    check_clean_image(volume)
    image = ExfatFixture(saved)
    assert image.read("srvros-work/anchor.txt") == b"workspace anchor\n"
    return image


def verify_desktop_workspace(stick, volume, release, keyboard_exit, file_checklist, mode):
    image = read_workspace(stick, volume)
    if file_checklist is not None:
        expected = file_checklist + (b"" if keyboard_exit else b"Ehci-42")
        assert image.read(f"srvros-work/ehci-desktop-{release}.txt") == expected
    report = "ehci-ethernet" if mode == "desktop-ethernet" else "ehci-trackpad"
    startup = image.read(f"srvros-work/{report}-{release}-startup.txt")
    final = image.read(f"srvros-work/{report}-{release}.txt")
    assert f"ehci-desktop-startup: release={release} phase=startup".encode() in startup
    assert f"ehci-desktop: release={release} result=INCOMPLETE".encode() in final
    if mode == "desktop-ethernet":
        assert b"ehci-desktop-ethernet: network init deferred until after USB checkpoint" in startup
        assert b"bcm57765-desktop: provider unavailable" in final
        assert b"ehci-desktop-ethernet-ui: result=INCOMPLETE phases=0" in final
    elif mode == "desktop-telemetry":
        assert b"pro81-telemetry: phase=startup cpu_state=1 cpu_fresh=0" in startup
        assert b"pro81-telemetry: phase=startup fan_state=1 fan_fresh=0" in startup
        assert b"pro81-telemetry: result=INCOMPLETE smc_quiesced=1" in final
        assert b"desktools: system-page=sensors" in final
    elif mode == "desktop-battery":
        assert b"pro81-battery: phase=startup state=5 fresh=0" in startup
        assert b"pro81-battery: result=INCOMPLETE smc_quiesced=1" in final
        assert b"desktools: system-page=sensors" in final
    elif file_checklist is None:
        assert b"pro81-backlight: provider=unavailable identity mismatch" in startup
        assert b"pro81-backlight: result=INCOMPLETE initial_duty=0 last_duty=0 writes=0" in final
    if mode == "desktop-audio":
        assert b"pro81-audio: phase=startup provider=unavailable" in startup
        assert b"pro81-audio: phase=final result=INCOMPLETE started=0 completed=0" in final


def exercise_ethernet_probe(command, temp, logs, release, log_volume, packets):
    stick, volume = desktop_workspace_image(temp, log_volume)
    command += ["-device", "qemu-xhci,id=xhci", "-drive", f"if=none,id=stick,format=raw,file={stick}",
                "-device", "usb-storage,drive=stick,bus=xhci.0"]
    serial = logs / "trial-serial.log"
    serial.unlink(missing_ok=True)
    with (logs / "trial-qemu.log").open("wb") as errors:
        with subprocess.Popen(command + ["-serial", f"file:{serial}"], stdout=errors, stderr=errors) as vm:
            try:
                text = wait_for(serial, lambda s: "Ethernet probe finished. Returning to Ubuntu." in s, vm, 90)
                assert vm.wait(timeout=20) == 0
            finally:
                if vm.poll() is None:
                    vm.kill()
                    vm.wait()
    assert "SRVROS_VM_NEXT_ENTRY_CONSUMED" in text and not has_fatal_exception(text)
    assert "ehci-ethernet: startup=saved final=saved workspace_retirement=safe" in text
    image = read_workspace(stick, volume)
    startup = image.read(f"srvros-work/ehci-ethernet-{release}-startup.txt").decode()
    final = image.read(f"srvros-work/ehci-ethernet-{release}.txt").decode()
    assert f"ehci-ethernet: release={release} phase=startup workspace=1" in startup
    assert f"ehci-ethernet: release={release} phase=final result=REFUSED packet_io={int(packets)}" in final
    assert "bcm57765: refused platform identity" in final
    assert all(marker not in final for marker in ("bcm57765: pci=", "bcm57765: phy_addr=", "bcm57765-packet:", "net: worker"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("release", help="exact staged EHCI trial release")
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    parser.add_argument("--stall-hub-status", action="store_true", help="inject a port 2 GET_STATUS stall")
    parser.add_argument("--keyboard-exit", action="store_true", help="finish the combined desktop trial without mouse input")
    args = parser.parse_args()
    if not boot.RELEASE_RE.fullmatch(args.release):
        parser.error("expected a 16-digit release identifier")
    release = args.release
    source = boot.STAGE / release
    manifest = boot.verify(source, release)
    version = manifest["config_version"]
    log_match = boot.EHCI_TRACKPAD_LOG_RE.fullmatch(version)
    desktop_match = boot.EHCI_DESKTOP_RE.fullmatch(version)
    ethernet_match = boot.EHCI_ETHERNET_RE.fullmatch(version)
    desktop_file = version.startswith((boot.EHCI_DESKTOP_FILE_VERSION, boot.EHCI_DESKTOP_FILE_V4_VERSION))
    brightness = version.startswith(boot.EHCI_DESKTOP_BRIGHTNESS_VERSION)
    audio = version.startswith((boot.EHCI_DESKTOP_AUDIO_VERSION, boot.EHCI_DESKTOP_AUDIO_V7_VERSION))
    telemetry = version.startswith(boot.EHCI_DESKTOP_TELEMETRY_VERSION)
    battery = version.startswith(boot.EHCI_DESKTOP_BATTERY_VERSION)
    desktop_ethernet = version.startswith(boot.EHCI_DESKTOP_ETHERNET_VERSION)
    file_checklist = boot.EHCI_DESKTOP_FILE_V4_CHECKLIST if version.startswith(boot.EHCI_DESKTOP_FILE_V4_VERSION) else boot.EHCI_DESKTOP_FILE_CHECKLIST
    desktop_trace = version.startswith(boot.EHCI_DESKTOP_VERSION)
    keyboard_checklist = desktop_trace or brightness or audio or version.startswith(boot.EHCI_DESKTOP_V2_VERSION)
    if args.keyboard_exit and not (desktop_trace or desktop_file or brightness or audio):
        parser.error("keyboard exit needs a v3 or later desktop stage")
    volume_match = log_match or desktop_match
    log_volume = ethernet_match[2] if ethernet_match else volume_match[1] if volume_match else None
    ethernet_packets = bool(ethernet_match and ethernet_match[1] == "2")
    if version not in boot.EHCI_TRIAL_VERSIONS and not log_volume:
        parser.error("expected an attended EHCI trial stage")
    keyboard_trial = version == boot.EHCI_KEYBOARD_VERSION
    keyboard_monitor = version == boot.EHCI_MONITOR_VERSION
    trackpad_trial = version == boot.EHCI_TRACKPAD_VERSION or bool(log_match)
    if (keyboard_monitor or trackpad_trial or desktop_match or ethernet_match) and args.stall_hub_status:
        parser.error("the emulated hub is for the split and typing trials")
    mode = ("ethernet-probe" if ethernet_match else "desktop-ethernet" if desktop_ethernet else "desktop-battery" if battery else
            "desktop-telemetry" if telemetry else "desktop-audio" if audio else
            "desktop-brightness" if brightness else "desktop-file" if desktop_file
            else "topcase-desktop" if desktop_trace else "trackpad-desktop" if desktop_match else
            "trackpad-trial" if log_match else boot.EHCI_TRIAL_MODES[version])
    logs = boot.ROOT / "build/review" / f"macbookpro8-1-ehci-trial-{release}"
    if args.keyboard_exit:
        logs /= "keyboard-exit"
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

        if ethernet_match:
            exercise_ethernet_probe(command.copy(), temp, logs, release, log_volume, ethernet_packets)
        elif keyboard_monitor:
            exercise_monitor(command.copy(), temp, logs, release)
        elif desktop_match:
            trial_command = command.copy()
            if desktop_file or brightness or audio or telemetry or battery or desktop_ethernet:
                stick, volume = desktop_workspace_image(temp, log_volume)
                trial_command += ["-device", "qemu-xhci,id=xhci", "-drive", f"if=none,id=stick,format=raw,file={stick}",
                                  "-device", "usb-storage,drive=stick,bus=xhci.0"]
            if audio:
                trial_command += ["-device", "ich9-intel-hda"]
            exercise_desktop(trial_command, temp, logs, release, log_volume, mode, keyboard_checklist, args.keyboard_exit, file_checklist)
            if desktop_file or brightness or audio or telemetry or battery or desktop_ethernet:
                verify_desktop_workspace(stick, volume, release, args.keyboard_exit,
                                         file_checklist if desktop_file else None, mode)
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
    if ethernet_match:
        result.pop("on_screen_checklist")
        result.update(ethernet_controller="refused", ethernet_packets=ethernet_packets,
                      startup_checkpoint="verified", final_report="verified")
    elif keyboard_monitor:
        result.update(monitor_input="serial", case_echo=True, history_edit=True, monitor_reboot=True)
    elif desktop_match:
        result.update(guest_trial_result="INCOMPLETE", desktop_ui="INCOMPLETE" if args.keyboard_exit else "PASS",
                      vm_mouse="unused" if args.keyboard_exit else "PS/2")
        if keyboard_checklist:
            result.update(vm_keyboard="PS/2", typed_checklist="vm-typed-checklist.ppm")
        if desktop_trace:
            result.update(startup_checkpoint="attempted", final_report="attempted", keyboard_exit=args.keyboard_exit)
        if desktop_file:
            result.update(startup_checkpoint="verified", final_report="verified", keyboard_exit=args.keyboard_exit,
                          vm_keyboard="PS/2", file_roundtrip="unmodified" if args.keyboard_exit else "verified")
        if brightness:
            result.update(startup_checkpoint="verified", final_report="verified", keyboard_exit=args.keyboard_exit,
                          brightness_provider="refused")
        if audio:
            result.update(startup_checkpoint="verified", final_report="verified", keyboard_exit=args.keyboard_exit,
                          audio_provider="refused")
        if telemetry:
            result.update(startup_checkpoint="verified", final_report="verified", vm_mouse="unused",
                          sensor_providers="refused", planned_exit=True)
        if battery:
            result.update(startup_checkpoint="verified", final_report="verified", vm_mouse="unused",
                          battery_provider="refused", desktop_ui="INCOMPLETE")
        if desktop_ethernet:
            result.update(startup_checkpoint="verified", final_report="verified", vm_mouse="unused",
                          ethernet_provider="refused", desktop_ui="INCOMPLETE")
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
