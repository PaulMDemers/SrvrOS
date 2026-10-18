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


def workspace_preferences(ui, serial, process, logs, restored=False):
    """Use Settings beside the initial Help window; exercise the real USB writer."""
    def shortcut(keys):
        ui.call("human-monitor-command", {"command-line": "sendkey " + keys})
        time.sleep(.3)
    shortcut("meta_l-spc")
    for c in "settings new app": ui.key("spc" if c == " " else c)
    ui.key("ret")
    wait_for(serial, lambda s: "desktools: starting role=settings" in s, process, 30)
    time.sleep(1)
    # Help is slot 0; Settings is slot 1, content origin (57,132).
    # Shorten its window, then scroll to the trackpad controls.
    ui.mouse(613-24, 688-56)
    ui.mouse(0, 0, 1); ui.mouse(0, -80, 1); ui.mouse(0, 0, 0)
    shortcut("pgdn")
    pointer=[613,608]
    def click(x,y):
        ui.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
        ui.mouse(0,0,1);ui.mouse(0,0,0)
    if not restored:
        click(545,430)  # Speed 5; the initial default is 3.
    click(110,478)  # First boot disables acceleration; second re-enables it.
    time.sleep(1)
    ui.call("screendump", {"filename": str(logs / ("workspace-settings-restored.ppm" if restored else "workspace-settings-saved.ppm"))})
    before=len(serial.read_text(errors="replace"))
    shortcut("ctrl-w")
    wait_for(serial, lambda s: "desktools: exited role=settings failure=0" in s[before:], process, 30)


def verify_workspace(stick, volume, acceleration):
    from exfat_test_image import ExfatFixture, check_clean_image
    with stick.open("rb") as stream:
        stream.seek(2048*512);saved=stream.read()
    volume.write_bytes(saved);check_clean_image(volume)
    fixture=ExfatFixture(saved)
    expected=("srvros-desktop-v2\ntext_size=16\nutc_offset_minutes=0\nclock_flags=3\n"
              f"wallpaper=0\npointer_speed=5\npointer_acceleration={acceleration}\n").encode()
    actual=fixture.read("srvros-work/desktop.conf")
    assert actual==expected, (actual,expected)
    return fixture


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
                        for c in value: ui.key({">":"dot","$":"4","?":"slash"}.get(c,keys.get(c,c.lower())),c.isupper() or c in ">$?")
                    time.sleep(.5)
                    ui.call("screendump",{"filename":str(logs/"native-hardware-checklist.ppm")})
                    workspace_preferences(ui,serial,process,logs)
                    before=len(serial.read_text(errors="replace"))
                    shortcut("meta_l-spc");type_text("Text new app");ui.key("ret")
                    wait_for(serial,lambda s:"window=1" in s[before:] and "desktools: starting role=text" in s[before:],process,30)
                    time.sleep(.7);type_text("Laptop trial AaZz09 saved on USB.")
                    shortcut("ctrl-s")
                    wait_for(serial,lambda s:"picker ready mode=2" in s,process,30);ui.key("ret")
                    wait_for(serial,lambda s:"file-result role=text op=2 status=0" in s,process,60)
                    shortcut("meta_l-spc");type_text("terminal new app");ui.key("ret")
                    wait_for(serial,lambda s:"deskterm: ready shell=" in s,process,30)
                    for command_text in ("pwd > terminal-home.txt", "pwd >> terminal-home.txt",
                                         "pwd > /missing-pwd-dir/out", "echo $? > terminal-pwd-status.txt"):
                        time.sleep(.5);type_text(command_text);ui.key("ret")
                    time.sleep(.5)
                    ui.call("screendump",{"filename":str(logs/"workspace-terminal.ppm")})
                    type_text("exit");ui.key("ret")
                    wait_for(serial,lambda s:"deskterm: shell exited status=0" in s,process,30)
                    shortcut("meta_l-w")
                    wait_for(serial,lambda s:"deskterm: exited failure=0" in s,process,30)
                    shortcut("meta_l-shift-3")
                    shot_log=wait_for(serial,lambda s:"deskd: screenshot saved status=0 path=" in s,process,90)
                    shot_path=re.search(r"deskd: screenshot saved status=0 path=(/work/srvros-work/Screenshots/[^\r\n]+)",shot_log)[1]
                    wait_for(serial,lambda s:"workspace-log: snapshot=session-" in s,process,30)
                    shortcut("f10");shortcut("end");shortcut("ret")
                    wait_for(serial,lambda s:"desktop-trial: desktop returned=yes USB unmounted=yes" in s and s.endswith("srv> "),process,60)
                    stored=verify_workspace(stick,volume,0)
                    (logs/"workspace-first.exfat").write_bytes(volume.read_bytes())
                    assert stored.read("srvros-work/Untitled.txt")==b"Laptop trial AaZz09 saved on USB."
                    assert stored.read("srvros-work/terminal-home.txt")==b"/work/srvros-work\n"*2
                    assert stored.read("srvros-work/terminal-pwd-status.txt")==b"1\n"
                    shot_bytes=stored.read(shot_path.removeprefix("/work/"))
                    assert shot_bytes[:2]==b"BM" and len(shot_bytes)==54+1280*800*4
                    assert struct.unpack_from("<II",shot_bytes,18)==(1280,800)
                    first_session=re.search(r"workspace-log: directory=(/work/[^ ]+) ",serial.read_text(errors="replace"))[1]
                    assert b"laptop-probe: unsupported chipset" in stored.read(first_session.removeprefix("/work/")+"/boot.log")
                    assert b"deskd: exited" in stored.read(first_session.removeprefix("/work/")+"/exit.log")
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
            if args.desktop_check:
                # A new VM has a fresh RAM root. Only the USB workspace survives.
                first_serial=logs/"hardware-grub-first-serial.log"
                first_serial.write_bytes(serial.read_bytes());serial.unlink()
                qmp_path.unlink(missing_ok=True)
                process=subprocess.Popen(command,stdout=errors,stderr=errors)
                try:
                    wait_for(serial,lambda s:"deskhelp: page=laptop-trial" in s and "deskd: opened" in s,process,args.boot_wait)
                    ui=QMP(qmp_path)
                    workspace_preferences(ui,serial,process,logs,restored=True)
                    for keys in ("f10","end","ret"):
                        ui.call("human-monitor-command",{"command-line":"sendkey "+keys});time.sleep(.3)
                    text=wait_for(serial,lambda s:"desktop-trial: desktop returned=yes USB unmounted=yes" in s and s.endswith("srv> "),process,60)
                    assert not has_fatal_exception(text)
                    stored=verify_workspace(stick,volume,1)
                    assert stored.read("srvros-work/Untitled.txt")==b"Laptop trial AaZz09 saved on USB."
                    assert stored.read(shot_path.removeprefix("/work/"))==shot_bytes
                    second_session=re.search(r"workspace-log: directory=(/work/[^ ]+) ",text)[1]
                    assert second_session!=first_session
                    from hardware_desktop import collect, FILES
                    recovered=temp/"recovered-usb"
                    (recovered/"srvros-work").mkdir(parents=True)
                    (recovered/"srvros-work/.workspace").write_bytes(stored.read("srvros-work/.workspace"))
                    for folder in (first_session,second_session):
                        relative=folder.removeprefix("/work/")
                        assert b"deskd: exited" in stored.read(relative+"/exit.log")
                        target=recovered/relative;target.mkdir(parents=True)
                        for filename in FILES:
                            try: contents=stored.read(relative+"/"+filename)
                            except StopIteration: continue
                            (target/filename).write_bytes(contents)
                    destination,report=collect(recovered,desktop[1],logs/"desktop-collections")
                    assert len(report["sessions"])==2
                    assert all(item["exit_snapshot_present"] and item["desktop_exit_seen"] and not item["attention"] for item in report["sessions"])
                    print(f"desktop diagnostics: two sessions collected and verified: {destination}")
                    (logs/"workspace-final.exfat").write_bytes(volume.read_bytes())
                    ui.stream.close();ui.sock.close()
                finally:
                    process.terminate()
                    try: process.wait(timeout=5)
                    except subprocess.TimeoutExpired: process.kill();process.wait()
    print(f"hardware-boot-smoke: PASS ({release}); GRUB chainload, RAM filesystem, USB input" + (", case/Caps/repeat/renderbench" if args.console_check else ""))
    if args.kernel_check:
        print("kernel: PASS (nested register preservation, timer/preemption, RAM fsck, cache service windows)")
    if args.responsiveness_check:
        print("responsiveness: PASS (USB input under output load, timer progress, latency samples, Escape, userspace Ctrl-C)")
    if args.desktop_check:
        print("Native desktop trial: checklist, USB keyboard editing, verified GUI save, default USB documents/screenshots, durable session logs, preferences across fresh VM boot and two clean unmounts passed.")
    print(f"Serial log: {serial}")


if __name__ == "__main__":
    main()
