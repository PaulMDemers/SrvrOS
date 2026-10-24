#!/usr/bin/env python3
"""Exercise the staged GRUB -> Limine -> kernel path on a disposable QEMU ESP."""
import argparse
import json
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


def workspace_preferences(ui, serial, process, logs, restored=False, expanded=False):
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
        click(545,430-(94 if expanded else 0))  # Speed 5; the initial default is 3.
    click(110,478-(94 if expanded else 0))  # First boot disables acceleration; second re-enables it.
    time.sleep(1)
    ui.call("screendump", {"filename": str(logs / ("workspace-settings-restored.ppm" if restored else "workspace-settings-saved.ppm"))})
    before=len(serial.read_text(errors="replace"))
    shortcut("ctrl-w")
    # Independent process logs can split the prefix ("desktools:"), while the
    # exit payload still identifies the exact role and successful result.
    wait_for(serial, lambda s: "exited role=settings failure=0" in s[before:], process, 30)


def verify_workspace(stick, volume, acceleration):
    from exfat_test_image import ExfatFixture, check_clean_image
    with stick.open("rb") as stream:
        stream.seek(2048*512);saved=stream.read(128*1024*1024)
    volume.write_bytes(saved);check_clean_image(volume)
    fixture=ExfatFixture(saved)
    expected=("srvros-desktop-v2\ntext_size=16\nutc_offset_minutes=0\nclock_flags=3\n"
              f"wallpaper=0\npointer_speed=5\npointer_acceleration={acceleration}\n").encode()
    actual=fixture.read("srvros-work/desktop.conf")
    if actual.startswith((b"srvros-desktop-v3\n",b"srvros-desktop-v4\n")):
        expected=expected.replace(b"srvros-desktop-v2",b"srvros-desktop-v3")+b"disable_while_typing=0\n"
    if actual.startswith(b"srvros-desktop-v4\n"):
        expected=expected.replace(b"srvros-desktop-v3",b"srvros-desktop-v4")+b"tap_to_click=1\ntwo_finger_tap=1\n"
    assert actual==expected, (actual,expected)
    return fixture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot-wait", type=float, default=180)
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--desktop-check", action="store_true")
    parser.add_argument("--desktop-no-headroom", action="store_true", help="Verify safe desktop refusal at the exFAT file-slot limit")
    parser.add_argument("--audio-check", action="store_true", help="Add HDA to the exact staged desktop and validate a captured test tone and bundled WAV playback")
    parser.add_argument("--audio-codec-timer", choices=("on","off"), default="off", help="QEMU codec timing; callback mode avoids host-load capture drops")
    parser.add_argument("--workspace-table", choices=("mbr", "gpt"), default="mbr")
    parser.add_argument("--keyboard-timeout", action="store_true", help="send no keys and verify the guided trial deadline")
    parser.add_argument("--keyboard-fail", action="store_true", help="inject wrong keys before correct keys in the guided typing rehearsal")
    parser.add_argument("--probe-check", action="store_true")
    parser.add_argument("--probe-missing-workspace", action="store_true")
    parser.add_argument("--console-check", action="store_true")
    parser.add_argument("--kernel-check", action="store_true")
    parser.add_argument("--responsiveness-check", action="store_true")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    args = parser.parse_args()
    logs = boot.ROOT / "build/review"
    logs.mkdir(parents=True, exist_ok=True)
    (logs/"hardware-grub-result.json").unlink(missing_ok=True)
    (logs/"hardware-grub-pcm.json").unlink(missing_ok=True)
    release = (boot.STAGE / "latest").read_text().strip()
    source = boot.STAGE / release
    manifest=boot.verify(source)
    desktop=boot.DESKTOP_RE.fullmatch(manifest.get("config_version", ""))
    if bool(desktop) != args.desktop_check:
        parser.error("Native desktop stages require --desktop-check; other stages cannot use it")
    if manifest.get("config_version", "").startswith(("ram-usb-v54-","ram-usb-v55-")):
        parser.error("Ordinary battery sessions use battery_home_desktop_smoke.py --boot-session --case profile-refusal --stage " + release + (" --services" if manifest["config_version"].startswith("ram-usb-v55-") else ""))
    basic_desktop=bool(desktop and manifest["config_version"].startswith("ram-usb-v53-"))
    if args.desktop_no_headroom and (not desktop or basic_desktop or args.audio_check):
        parser.error("--desktop-no-headroom requires an ordinary desktop stage without audio checking")
    session_run="0123456789abcdef"
    probe=boot.LAPTOP_PROBE_RE.fullmatch(manifest.get("config_version", ""))
    if bool(probe) != args.probe_check or (args.probe_missing_workspace and not probe):
        parser.error("Automatic laptop probe stages require --probe-check")
    if args.audio_check and (not desktop or not manifest["config_version"].startswith(("ram-usb-v21-", "ram-usb-v22-", "ram-usb-v58-", "ram-usb-v59-", "ram-usb-v31-", "ram-usb-v36-", "ram-usb-v37-", "ram-usb-v57-"))):
        parser.error("--audio-check requires an audio-enabled desktop stage")
    keyboard_capture=bool(probe and manifest["config_version"].startswith(("ram-usb-v27-","ram-usb-v28-")))
    if args.keyboard_fail and args.keyboard_timeout: parser.error("choose wrong input or timeout")
    keyboard_retry=bool(probe and manifest["config_version"].startswith("ram-usb-v28-"))
    keyboard_negative=args.keyboard_timeout or (args.keyboard_fail and not keyboard_retry)
    if keyboard_negative and not keyboard_capture: parser.error("--keyboard-fail requires keyboard capture")
    cpu_load_capture=bool(probe and manifest["config_version"].startswith("ram-usb-v35-"))
    idle_capture=bool(probe and manifest["config_version"].startswith("ram-usb-v23-"))
    desktop_idle_capture=bool(probe and manifest["config_version"].startswith(("ram-usb-v25-","ram-usb-v26-")))
    battery_preflight=bool(probe and manifest["config_version"].startswith("ram-usb-v60-"))
    power_telemetry=bool(probe and manifest["config_version"].startswith(("ram-usb-v26-","ram-usb-v35-")))
    auto_return=manifest["config_version"].endswith("-auto-return")
    battery_home_transitions=bool(probe and manifest["config_version"].startswith("ram-usb-v52-"))
    battery_home_desktop=bool(probe and manifest["config_version"].startswith("ram-usb-v51-"))
    battery_home_session=bool(probe and manifest["config_version"].startswith(("ram-usb-v49-","ram-usb-v66-")))
    battery_home=bool(probe and manifest["config_version"].startswith(("ram-usb-v48-","ram-usb-v65-")))
    battery_smbus_transitions=bool(probe and manifest["config_version"].startswith("ram-usb-v63-"))
    battery_smbus_files=bool(probe and manifest["config_version"].startswith(("ram-usb-v47-","ram-usb-v64-")))
    battery_smbus=bool(probe and manifest["config_version"].startswith(("ram-usb-v46-","ram-usb-v62-")))
    battery_query=bool(probe and manifest["config_version"].startswith("ram-usb-v45-"))
    battery_controls=bool(probe and manifest["config_version"].startswith("ram-usb-v44-"))
    battery_app_files=bool(probe and manifest["config_version"].startswith("ram-usb-v43-"))
    battery_load=bool(probe and manifest["config_version"].startswith("ram-usb-v42-"))
    battery_text=bool(probe and manifest["config_version"].startswith("ram-usb-v41-"))
    battery_desktop=bool(probe and manifest["config_version"].startswith("ram-usb-v40-"))
    battery_service=bool(probe and manifest["config_version"].startswith(("ram-usb-v33-","ram-usb-v34-","ram-usb-v36-","ram-usb-v38-")))
    battery_transitions=bool(probe and manifest["config_version"].startswith("ram-usb-v32-"))
    battery_capture=bool(probe and manifest["config_version"].startswith(("ram-usb-v31-","ram-usb-v32-","ram-usb-v33-","ram-usb-v34-","ram-usb-v36-","ram-usb-v38-","ram-usb-v40-","ram-usb-v41-","ram-usb-v42-","ram-usb-v43-","ram-usb-v44-","ram-usb-v45-","ram-usb-v46-","ram-usb-v47-","ram-usb-v48-","ram-usb-v49-","ram-usb-v51-","ram-usb-v52-","ram-usb-v61-","ram-usb-v62-","ram-usb-v63-","ram-usb-v64-","ram-usb-v65-","ram-usb-v66-")))
    battery_filesystem=battery_smbus_files or battery_app_files or battery_load or bool(probe and manifest["config_version"].startswith("ram-usb-v38-"))
    battery_usb_write=bool(probe and manifest["config_version"].startswith("ram-usb-v36-"))
    battery_marker="battery-smbus-transitions-capture" if battery_smbus_transitions else "battery-home-transitions-capture" if battery_home_transitions else "battery-home-desktop-capture" if battery_home_desktop else "battery-home-session-capture" if battery_home_session else "battery-home-capture" if battery_home else "battery-smbus-files-capture" if battery_smbus_files else "battery-smbus-capture" if battery_smbus else "battery-query-capture" if battery_query else "battery-controls-capture" if battery_controls else "battery-app-files-capture" if battery_app_files else "battery-load-capture" if battery_load else "battery-text-capture" if battery_text else "battery-desktop-capture" if battery_desktop else "battery-service-capture" if battery_service else "battery-transition-capture" if battery_transitions else "battery-capture"
    aml_capture=bool(probe and manifest["config_version"].startswith("ram-usb-v24-"))
    scaled_desktop=bool(desktop and manifest["config_version"].startswith(("ram-usb-v29-","ram-usb-v30-","ram-usb-v31-", "ram-usb-v36-", "ram-usb-v37-")))
    workspace=desktop or probe
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
        if scaled_desktop:
            command += ["-vga","none","-device","VGA,vgamem_mb=64,xres=2880,yres=1800"]
        network_desktop=bool(desktop and manifest["config_version"].startswith("ram-usb-v37-"))
        device_checks=bool(desktop and manifest["config_version"].startswith("ram-usb-v58-"))
        if network_desktop or device_checks:
            command += ["-nic","none","-netdev","user,id=ecm","-device","usb-net,netdev=ecm,bus=xhci.0,id=usbnet"]
        if args.audio_check:
            command += ["-device","ich9-intel-hda","-device",f"hda-duplex,audiodev=capture,use-timer={args.audio_codec_timer}","-trace","enable=hda_audio_overrun",
                "-audiodev",f"wav,id=capture,path={logs/'hardware-grub-tone.wav'},out.frequency=48000"]
        if workspace and not args.probe_missing_workspace:
            from exfat_test_image import ExfatFixture, check_clean_image
            volume=temp/"workspace.exfat";stick=temp/"workspace.img"
            with volume.open("wb") as stream: stream.truncate(128*1024*1024)
            subprocess.run(["/usr/sbin/mkfs.exfat","-c","128K",str(volume)],check=True,stdout=subprocess.DEVNULL)
            subprocess.run(["/usr/sbin/tune.exfat","-I","0x"+workspace[1],str(volume)],check=True,stdout=subprocess.DEVNULL)
            fixture=ExfatFixture(volume.read_bytes())
            files={".workspace":f"SRVROS-WORKSPACE-V1 {workspace[1]}\n".encode()}
            if battery_filesystem:
                from battery_filesystem_image import create
                inner=temp/'inner.exfat';create(inner)
                files['battery-fs.img']=inner.read_bytes()
            if basic_desktop:
                from desktop_session_workload import files as session_files
                files.update(session_files(session_run))
                from desktop_session_workload import refusal_files
                files.update(refusal_files(session_run, session_run))
                files.update({"collision-a.txt":b"saved alpha\n", "collision-b.txt":b"saved beta\n"})
                metadata="SRVROSMETA1\n"+"".join(f"/work/srvros-work/{name}\t50000\t33188\t1\t0\t0\t100\t200\t300\n" for name in ("collision-a.txt","collision-b.txt"))
                fixture.add_directory_files(".srvros",{"meta":metadata.encode()})
            if args.desktop_no_headroom:
                from hardware_log_archive import limits
                files.update({f"fill{i:04d}":b"" for i in range(limits()[0]-3)})
                fixture.add_directory_files(".srvros",{"meta":b"SRVROSMETA1\n"})
            fixture.add_directory_files("srvros-work",files)
            volume.write_bytes(fixture.data);check_clean_image(volume)
            mbr=bytearray(512);mbr[450]=7
            struct.pack_into("<II",mbr,454,2048,len(fixture.data)//512);mbr[510:]=b"\x55\xaa"
            if args.workspace_table == "gpt":
                last = 2048 + len(fixture.data)//512 - 1
                backup = last + 33
                entries = bytearray(128*128)
                entries[:128] = gpt_entry(uuid.UUID("ebd0a0a2-b9e5-4433-87c0-68b6b72699c7"),
                    uuid.uuid4(), 2048, last, "Workspace")
                crc, guid = binascii.crc32(entries) & 0xffffffff, uuid.uuid4()
                mbr[450] = 0xee
                # Match the saturated protective length captured on the Pro USB.
                struct.pack_into("<II", mbr, 454, 1, 0xffffffff)
                with stick.open("wb") as stream:
                    stream.write(mbr)
                    stream.write(gpt_header(1, backup, 34, last, 2, 128, crc, guid))
                    stream.write(entries)
                    stream.seek(2048*512);stream.write(fixture.data)
                    stream.write(entries)
                    stream.write(gpt_header(backup, 1, 34, last, backup-32, 128, crc, guid))
            else:
                with stick.open("wb") as stream: stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
            command += ["-drive",f"if=none,id=stick,format=raw,file={stick}","-device","usb-storage,drive=stick,bus=xhci.0"]
        with (logs / "hardware-grub-qemu.log").open("wb") as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                if args.desktop_no_headroom:
                    text=wait_for(serial,lambda s:"desktop-trial: NOT STARTED" in s and "srv> " in s,process,args.boot_wait)
                    assert "workspace-log: REFUSED insufficient file/directory slots" in text
                    assert "desktop-trial: USB unmounted=yes" in text
                    assert "/fat/bin/deskd" not in text
                    with stick.open("rb") as stream:
                        stream.seek(2048*512);saved=stream.read(len(fixture.data))
                    volume.write_bytes(saved);check_clean_image(volume)
                    final=ExfatFixture(saved)
                    entries=dict(final.entries(final.read("srvros-work")))
                    assert set(entries)==set(files), set(entries)-set(files)
                    for name,payload in files.items():
                        entry=entries[name]
                        first=struct.unpack_from('<I',entry,52)[0]
                        size=struct.unpack_from('<Q',entry,56)[0]
                        assert final.chain(first,size,bool(entry[33]&2))==payload
                    result=dict(status="PASS",release=release,manifest_sha256=boot.digest(source/"manifest.json"),
                                desktop_no_headroom=True,workspace_table=args.workspace_table)
                    (logs/"hardware-grub-result.json").write_text(json.dumps(result,indent=2)+"\n")
                    print(f"hardware-grub-smoke: PASS ({release}); capacity refusal, no session created, clean unmount")
                    return
                if args.probe_check:
                    if args.probe_missing_workspace:
                        text=wait_for(serial,lambda s:("laptop-capture: returning to Ubuntu" if auto_return else "laptop-capture: HALTED") in s,process,args.boot_wait)
                        if auto_return:
                            assert process.wait(timeout=30)==0
                        else:
                            time.sleep(2)
                            assert process.poll() is None and "laptop-capture: returning" not in text
                        assert "laptop-capture: saved=NO unmounted=NO" in text
                        print(f"laptop-probe-smoke: PASS ({release}); missing workspace " + ("returns without starting probe" if auto_return else "halts without reboot"))
                        return
                    if keyboard_capture:
                        wait_for(serial, lambda s: "keyboard-test: step=1 " in s, process, args.boot_wait)
                        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as qmp:
                            qmp.settimeout(5);qmp.connect(str(qmp_path));qmp.recv(4096)
                            qmp_command(qmp, {"execute":"qmp_capabilities"})
                            qmp_command(qmp, {"execute":"screendump", "arguments":{"filename":str(logs/"keyboard-instructions.ppm")}})
                            keys=[("a",False),("a",True),("a",False),("a",False),("backspace",False),
                                  ("ret",False),("up",False),("down",False),("left",False),("right",False)]
                            if args.keyboard_fail and not keyboard_retry: send_qmp_key(qmp,"b")
                            elif not args.keyboard_timeout:
                                for i,(key,shift) in enumerate(keys,1):
                                    wait_for(serial,lambda s: f"keyboard-test: step={i} " in s,process,40)
                                    if args.keyboard_fail and keyboard_retry:
                                        send_qmp_key(qmp,"b");time.sleep(.2)
                                        wait_for(serial,lambda s:f"keyboard-test: retry step={i} " in s,process,10)
                                    time.sleep(.2) # Release the previous HID report before the next step.
                                    if i in (3,4):
                                        send_qmp_key(qmp,"caps_lock");time.sleep(.2)
                                    send_qmp_key(qmp,key,shift)
                    if battery_capture:
                        text=wait_for(serial,lambda s:("laptop-capture: returning to Ubuntu" if auto_return else "laptop-capture: HALTED") in s,process,args.boot_wait)
                        assert "battery: REFUSED platform/resource ownership preflight" in text
                        assert battery_marker+": returned=yes clean=NO" in text
                        assert "laptop-capture: saved=yes unmounted=yes" in text
                        assert "laptop-capture: completed=NO" in text
                        assert "battery-native: REFUSED check=pci-gpu" in text
                        assert "battery: phase=" not in text
                        if battery_home_desktop or battery_home_transitions:
                            assert "battery-workspace-controls: complete=NO" in text
                            assert "battery-home-desktop: logger_retired=yes final_deferred=yes" in text
                            assert text.index("battery-workspace-hold: released") < text.index("workspace: unmounted; USB writes disabled")
                            assert text.count("workspace: ready at") == 1
                        elif battery_home or battery_home_session:
                            assert "laptop-capture: battery Home mounted=yes logging-paused=yes fault-reset=disabled" in text
                            assert ("battery-workspace-controls: complete=NO" if battery_home_session else "battery-home: outcome=unavailable") in text
                            assert text.index("battery-workspace-hold: released") < text.index("workspace: unmounted; USB writes disabled")
                            assert text.count("workspace: ready at") == 1
                        elif auto_return:
                            assert "laptop-capture: battery media parked=yes fault-reset=armed" in text
                            assert text.index("workspace: unmounted; USB writes disabled") < text.index("battery: REFUSED")
                            assert text.count("workspace: ready at") == 2
                        else:
                            assert "laptop-capture: returning" not in text
                            from desktop_foundation_smoke import QMP
                            ui=QMP(qmp_path);ui.call("quit");process.wait(timeout=10)
                    else:
                        text=wait_for(serial,lambda s:"laptop-capture: returning to Ubuntu" in s,process,args.boot_wait)
                        assert "laptop-capture: saved=yes unmounted=yes" in text
                    if battery_preflight: assert "battery-preflight: REFUSED identity/resources; no register access" in text
                    elif not (idle_capture or aml_capture or desktop_idle_capture or battery_capture or cpu_load_capture): assert "laptop-probe: unsupported chipset; no register access" in text
                    if keyboard_retry and args.keyboard_fail: assert "keyboard-test: retries=10" in text
                    if keyboard_retry and args.keyboard_timeout: assert "keyboard-test: timeout step=1 limit_seconds=90" in text
                    if keyboard_capture:
                        assert ("keyboard-test: result=FAIL passed=0" if keyboard_negative else "keyboard-test: result=PASS passed=10") in text
                    if args.workspace_table == "gpt":
                        assert "block: registered GPT partition usb0p1" in text
                    if idle_capture:
                        measured=re.search(r"idle-capture: elapsed_us=(\d+) sampled_ticks=(\d+) idle_ticks=(\d+).*completed=yes",text)
                        assert measured and int(measured[1])>=59000000
                        assert 0<int(measured[3])<=int(measured[2])
                        assert "srvros.aml=probe" not in text
                    if desktop_idle_capture:
                        from hardware_desktop import desktop_idle_sample
                        assert desktop_idle_sample(text) is not None
                        assert "srvros.aml=probe" not in text and "deskd: exited" in text
                    assert not has_fatal_exception(text)
                    assert process.wait(timeout=30)==0 # -no-reboot exits only after guest reset.
                    with stick.open("rb") as stream:
                        stream.seek(2048*512);volume.write_bytes(stream.read(128*1024*1024))
                    check_clean_image(volume);stored=ExfatFixture(volume.read_bytes())
                    folder=re.search(r"workspace-log: directory=(/work/[^ ]+) ",text)[1].removeprefix("/work/")
                    for filename in ("boot.log","exit.log"):
                        contents=stored.read(folder+"/"+filename)
                        assert release.encode() in contents
                        if keyboard_capture and filename=="exit.log":
                            assert (b"keyboard-test: result=FAIL passed=0" if keyboard_negative else b"keyboard-test: result=PASS passed=10") in contents
                        if not (idle_capture or aml_capture or desktop_idle_capture or battery_capture or cpu_load_capture):
                            assert (b"battery-preflight: REFUSED identity/resources; no register access" if battery_preflight else b"laptop-probe: unsupported chipset") in contents
                            assert b"USB INPUT DIAGNOSTIC" in contents
                            assert b"xhci: vendor=" in contents
                            assert b"input-usb: dev slot=" in contents
                            assert b"Expected internal keyboard: 05ac:0291" not in contents
                        (logs/("laptop-capture-"+filename)).write_bytes(contents)
                    assert b"laptop-capture: diagnostics collected" in stored.read(folder+"/exit.log")
                    if battery_preflight:
                        exit_log=stored.read(folder+"/exit.log").decode()
                        from hardware_desktop import battery_preflight_result
                        assert battery_preflight_result(exit_log) is None
                        assert "battery-preflight: complete" not in exit_log
                        assert "aml-probe:" not in exit_log and "battery-native:" not in exit_log
                    if battery_capture:
                        exit_log=stored.read(folder+"/exit.log")
                        assert (battery_marker+": returned=yes clean=NO").encode() in exit_log
                        assert b"battery: REFUSED platform/resource ownership preflight" in exit_log
                    if battery_filesystem:
                        exit_log=stored.read(folder+"/exit.log")
                        assert b"workspace-image: revoked clean=yes" in exit_log
                        assert stored.read('srvros-work/battery-fs.img')==inner.read_bytes()
                        assert b"battery-filesystem: PASS" not in exit_log
                    if battery_usb_write:
                        exit_log=stored.read(folder+"/exit.log")
                        assert b"workspace-scratch: revoked clean=yes" in exit_log
                        prepared=re.search(rb"workspace-scratch: prepared path=(/work/srvros-work/battery-io-[0-9a-f]{16}\.bin) raw_offset=\d+ bytes=4096; unmounted=yes aliases=revoked",exit_log)
                        assert prepared and len(stored.read(prepared[1].decode().removeprefix("/work/")))==4096
                        assert b"battery-usb-write: PASS" not in exit_log
                    if cpu_load_capture:
                        exit_log=stored.read(folder+"/exit.log").decode()
                        assert "cpu-load: completed=NO reason=thermal-unavailable-or-stale" in exit_log
                        assert "cpu-load: completed=yes" not in exit_log and "cpu-load: phase=load" not in exit_log
                        assert "cpu-load-capture: returned=yes" in exit_log
                        assert "srvros.aml" not in exit_log
                        from hardware_desktop import device_summaries
                        power=device_summaries(exit_log)["power"]
                        assert power["power_state"]==1 and power["samples"]==0 and power["flags"]==0
                        from hardware_cpu_load import load_result
                        assert load_result(exit_log)["completed"] is False
                    if aml_capture:
                        exit_log=stored.read(folder+"/exit.log")
                        assert b"aml-capture: returned=yes clean=yes" in exit_log
                        assert re.search(rb"aml-probe: completed .*errors=0 denied=0 .*heap_remaining=0",exit_log)
                    if idle_capture:
                        exit_log=stored.read(folder+"/exit.log")
                        assert b"idle-capture: elapsed_us=" in exit_log and b"completed=yes" in exit_log
                        assert exit_log.count(b"workspace-scheduler:")>=5
                        for filename in ("session-a.log","session-b.log"):
                            contents=stored.read(folder+"/"+filename)
                            assert b"workspace-scheduler:" in contents
                            (logs/("laptop-capture-"+filename)).write_bytes(contents)
                    if desktop_idle_capture:
                        exit_log=stored.read(folder+"/exit.log").decode()
                        assert desktop_idle_sample(exit_log) is not None
                        if power_telemetry:
                            from hardware_desktop import power_idle_sample,device_summaries
                            power=device_summaries(exit_log)["power"]
                            assert power["power_state"]==1 and power["samples"]==0 and power["flags"]==0
                            assert "srvros.power=read-only" in exit_log
                            assert "desktop-idle-window: start_us=" in exit_log
                            assert power_idle_sample(exit_log) is None # Refused VM is never a power measurement.
                            if 'worker-snapshot:' in exit_log:
                                from hardware_desktop import desktop_worker_sample
                                workers=desktop_worker_sample(exit_log)
                                assert workers is not None, 'Incomplete worker attribution'
                                (logs/'desktop-workers.json').write_text(json.dumps(workers,indent=2)+'\n')
                        capture=re.findall(r"deskd: screenshot saved status=0 path=(/work/[^\s]+)",exit_log)
                        assert len(capture)==1
                        image=stored.read(capture[0].removeprefix("/work/"))
                        assert image[:2]==b"BM" and len(image)>54
                        import io
                        from PIL import Image
                        decoded=Image.open(io.BytesIO(image));decoded.load()
                        assert decoded.width>=640 and decoded.height>=480
                        (logs/"desktop-idle.bmp").write_bytes(image)
                    (logs/"hardware-grub-result.json").write_text(json.dumps({
                        "status":"PASS","release":release,"manifest_sha256":boot.digest(source/"manifest.json"),
                        "battery_home_transitions":battery_home_transitions,"battery_home_desktop":battery_home_desktop,"battery_home_session":battery_home_session,"battery_home":battery_home,"battery_smbus_files":battery_smbus_files,"battery_smbus":battery_smbus,"battery_smbus_transitions":battery_smbus_transitions,"battery_query":battery_query,"battery_controls":battery_controls,"battery_app_files":battery_app_files,"battery_load":battery_load,"battery_text":battery_text,"battery_desktop":battery_desktop,"battery_capture":battery_capture,"battery_transitions":battery_transitions,"battery_service":battery_service,"battery_filesystem":battery_filesystem,"return_on_failure":auto_return,"battery_outcome":"expected platform refusal" if battery_capture else "not requested",
                        "probe":True,"battery_preflight":battery_preflight,"preflight_outcome":"expected identity refusal" if battery_preflight else "not requested","cpu_load_capture":cpu_load_capture,"load_result":"refused in VM" if cpu_load_capture else "not requested","power_telemetry":power_telemetry,"power_measurement":"unavailable in VM" if power_telemetry else "not requested","desktop_idle_capture":desktop_idle_capture,"idle_capture":idle_capture,"aml_capture":aml_capture,"saved":True,"unmounted":True,
                        "keyboard_capture":keyboard_capture,"keyboard_expected_failure":keyboard_negative,"workspace_table":args.workspace_table,"physical_acceptance":"pending"},indent=2)+"\n")
                    (logs/"laptop-capture.exfat").write_bytes(volume.read_bytes())
                    print(f"laptop-probe-smoke: PASS ({release}); diagnostics saved, offline file bytes verified, clean exFAT; " + (("native battery refused in VM and returned" if auto_return else "native battery refused in VM and halted") if battery_capture else "automatic reset"))
                    return
                if args.desktop_check:
                    from desktop_foundation_smoke import QMP
                    wait_for(serial,lambda s:("deskterm: ready" if network_desktop else "deskhelp: page=laptop-trial") in s and "deskd: opened" in s,process,args.boot_wait)
                    ui=QMP(qmp_path)
                    def shortcut(keys):
                        ui.call("human-monitor-command",{"command-line":"sendkey "+keys});time.sleep(.3)
                    def type_text(value):
                        keys={" ":"spc","/":"slash",".":"dot","-":"minus"}
                        for c in value: ui.key({">":"dot","$":"4","?":"slash",";":"semicolon"}.get(c,keys.get(c,c.lower())),c.isupper() or c in ">$?")
                    if network_desktop:
                        # Each saved phase is displayed by cat before its next gate.
                        def phase_cats(count):
                            wait_for(serial,lambda s:s.count('/fat/bin/cat entry=')>=count,process,90)
                            time.sleep(2)
                        phase_cats(1)
                        ui.call('screendump',{'filename':str(logs/'network-auto-unplug.ppm')})
                        initial_sleeps=serial.read_text(errors='replace').count('/fat/bin/sleep entry=')
                        ui.call('set_link',{'name':'usbnet','up':False});ui.key('ret')
                        phase_cats(2)
                        ui.call('screendump',{'filename':str(logs/'network-auto-reconnect.ppm')})
                        assert serial.read_text(errors='replace').count('/fat/bin/sleep entry=')==initial_sleeps+1
                        ui.call('set_link',{'name':'usbnet','up':True});ui.key('ret')
                        wait_for(serial,lambda s:'deskterm: shell exited status=0' in s,process,90)
                        ui.call('screendump',{'filename':str(logs/'network-auto-complete.ppm')})
                        shortcut('meta_l-w')
                        shortcut('meta_l-spc');type_text('Help new app');ui.key('ret')
                        wait_for(serial,lambda s:'deskhelp: starting' in s,process,30)
                    if scaled_desktop:
                        text=serial.read_text(errors="replace")
                        assert "physical=2880x1800 logical=1440x900 scale=2" in text
                        time.sleep(1)
                        ui.call("screendump",{"filename":str(logs/"native-scaled-checklist.ppm")})
                        if manifest["config_version"].startswith(("ram-usb-v36-","ram-usb-v37-")):
                            shortcut("meta_l-spc");type_text("System new app");ui.key("ret")
                            wait_for(serial,lambda s:"desktools: system-ready" in s,process,30)
                        if network_desktop:
                            shortcut("meta_l-spc");type_text("Terminal new app");ui.key("ret")
                            wait_for(serial,lambda s:"deskterm: starting" in s,process,30)
                            time.sleep(2);type_text("dhcp");ui.key("ret")
                            wait_for(serial,lambda s:"net: dhcp ipv4=10.0.2.15" in s,process,30)
                            type_text("ping 10.0.2.2 > /work/srvros-work/network-ping.txt");ui.key("ret")
                            time.sleep(5);type_text("exit");ui.key("ret")
                            wait_for(serial,lambda s:"deskterm: shell exited" in s,process,30)
                            shortcut("meta_l-w")
                            wait_for(serial,lambda s:"deskterm: exited failure=0" in s,process,30)
                        shortcut("meta_l-spc");type_text("Text new app");ui.key("ret")
                        wait_for(serial,lambda s:"desktools: starting role=text" in s,process,30)
                        time.sleep(1);type_text("Pro scaled desktop USB test.");shortcut("ctrl-s")
                        wait_for(serial,lambda s:"picker ready mode=2" in s,process,30);ui.key("ret")
                        wait_for(serial,lambda s:"file-result role=text op=2 status=0" in s,process,60)
                        shortcut("meta_l-shift-3")
                        text=wait_for(serial,lambda s:"deskd: screenshot saved status=0 path=/work/" in s,process,60)
                        shot=re.search(r"deskd: screenshot saved status=0 path=(/work/[^\s]+)",text)[1]
                        for keys in ("f10","end","ret"): shortcut(keys)
                        text=wait_for(serial,lambda s:"desktop-trial: desktop returned=yes USB unmounted=yes" in s and s.endswith("srv> "),process,60)
                        with stick.open("rb") as stream:
                            stream.seek(2048*512);volume.write_bytes(stream.read(128*1024*1024))
                        check_clean_image(volume);stored=ExfatFixture(volume.read_bytes())
                        assert stored.read("srvros-work/Untitled.txt")==b"Pro scaled desktop USB test."
                        if network_desktop: assert b"sent=4 received=4 lost=0" in stored.read("srvros-work/network-ping.txt")
                        if network_desktop:
                            assert stored.read('srvros-work/network-trial.txt')==b'started\ncompleted\n'
                            assert b'disconnected-dhcp-status=1' in stored.read('srvros-work/network-down.txt')
                            for phase in ('before','after'):
                                report=stored.read('srvros-work/network-'+phase+'.txt')
                                assert b'dhcp-status=0' in report and b'Network check complete' in report
                                (logs/('network-auto-'+phase+'.txt')).write_bytes(report)

                        bitmap=stored.read(shot.removeprefix("/work/"))
                        assert bitmap[:2]==b"BM" and struct.unpack_from("<II",bitmap,18)==(1440,900)
                        folder=re.search(r"workspace-log: directory=(/work/[^ ]+)",text)[1].removeprefix("/work/")
                        for name in ("boot.log","exit.log"):
                            data=stored.read(folder+"/"+name);assert release.encode() in data
                            (logs/("scaled-desktop-"+name)).write_bytes(data)
                        assert b"deskd: exited" in stored.read(folder+"/exit.log")
                        assert not has_fatal_exception(text)
                        tone_result=None
                        if args.audio_check:
                            assert b"audio: state=2" in stored.read(folder+"/exit.log")
                            offset=len(text)
                            type_text("audio test");ui.key("ret")
                            wait_for(serial,lambda s:"audio: request queued" in s[offset:],process,20)
                            time.sleep(1.5)
                            type_text("audio");ui.key("ret")
                            wait_for(serial,lambda s:"tests_started=1 tests_completed=1" in s[offset:] and s.endswith("srv> "),process,20)
                            ui.call("quit");process.wait(timeout=10)
                            from audio_smoke import analyze_tones
                            tone_result=analyze_tones(logs/"hardware-grub-tone.wav",1)
                        (logs/"hardware-grub-result.json").write_text(json.dumps({"status":"PASS","release":release,
                            "manifest_sha256":boot.digest(source/"manifest.json"),"desktop":True,"scale":2,"usb_network":network_desktop,"audio":args.audio_check,"tone":tone_result,
                            "workspace_table":args.workspace_table,"saved":True,"unmounted":True,"physical_acceptance":"pending"},indent=2)+"\n")
                        print(f"scaled-desktop-smoke: PASS ({release}); 2880x1800 at2x, saved Text/screenshot/logs, clean USB unmount")
                        return
                    time.sleep(.5)
                    ui.call("screendump",{"filename":str(logs/"native-hardware-checklist.ppm")})
                    workspace_preferences(ui,serial,process,logs,expanded=manifest.get("config_version", "").startswith(("ram-usb-v20-", "ram-usb-v21-", "ram-usb-v22-", "ram-usb-v58-", "ram-usb-v59-", "ram-usb-v53-", "ram-usb-v56-", "ram-usb-v57-")))
                    if basic_desktop:
                        shortcut("meta_l-spc");type_text("terminal new app");ui.key("ret")
                        wait_for(serial,lambda s:"deskterm: ready shell=" in s,process,30)
                        time.sleep(.7)
                        type_text(f"sh /work/srvros-work/session-{session_run}.sh; exit $?");ui.key("ret")
                        workload_started=time.monotonic()
                    before=len(serial.read_text(errors="replace"))
                    shortcut("meta_l-spc");type_text("Text new app");ui.key("ret")
                    wait_for(serial,lambda s:("window=2" if basic_desktop else "window=1") in s[before:] and "desktools: starting role=text" in s[before:],process,30)
                    time.sleep(.7);type_text("Laptop trial AaZz09 saved on USB.")
                    shortcut("ctrl-s")
                    wait_for(serial,lambda s:"picker ready mode=2" in s,process,30);ui.key("ret")
                    wait_for(serial,lambda s:"file-result role=text op=2 status=0" in s,process,60)
                    if basic_desktop:
                        before=len(serial.read_text(errors="replace"));shortcut("ctrl-o")
                        wait_for(serial,lambda s:"picker ready mode=1" in s[before:],process,30)
                        before=len(serial.read_text(errors="replace"));shortcut("alt-up")
                        wait_for(serial,lambda s:"picker ready mode=1" in s[before:],process,30)
                        before=len(serial.read_text(errors="replace"));type_text("srvros-work");ui.key("ret")
                        wait_for(serial,lambda s:"picker ready mode=1" in s[before:],process,30)
                        before=len(serial.read_text(errors="replace"));type_text("untitled");ui.key("ret")
                        wait_for(serial,lambda s:"file-result role=text op=1 status=0" in s[before:],process,30)
                    shortcut("meta_l-spc");type_text("terminal new app");ui.key("ret")
                    wait_for(serial,lambda s:"deskterm: ready shell=" in s,process,30)
                    if basic_desktop:
                        time.sleep(.7);type_text(f"sh /work/srvros-work/session-{session_run}.sh; echo $? > replay-active.txt");ui.key("ret")
                    for command_text in ("pwd > terminal-home.txt", "pwd >> terminal-home.txt",
                                         "pwd > /missing-pwd-dir/out", "echo $? > terminal-pwd-status.txt"):
                        time.sleep(.5);type_text(command_text);ui.key("ret")
                    if device_checks:
                        type_text("sh -c dhcp > device-dhcp.txt");ui.key("ret")
                        wait_for(serial,lambda s:"net: dhcp ipv4=10.0.2.15" in s,process,30)
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
                    if basic_desktop:
                        # The foreground workload keeps its Terminal alive while Text saves,
                        # the second Terminal writes, and screenshot capture runs independently.
                        assert time.monotonic()-workload_started < 120, "Input did not overlap the paced workload"
                        wait_for(serial,lambda s:s.count("deskterm: shell exited status=0") == 2,process,240)
                    shortcut("f10");shortcut("end");shortcut("ret")
                    wait_for(serial,lambda s:"desktop-trial: desktop returned=yes USB unmounted=yes" in s and s.endswith("srv> "),process,60)
                    stored=verify_workspace(stick,volume,0)
                    (logs/"workspace-first.exfat").write_bytes(volume.read_bytes())
                    assert stored.read("srvros-work/Untitled.txt")==b"Laptop trial AaZz09 saved on USB."
                    assert stored.read("srvros-work/terminal-home.txt")==b"/work/srvros-work\n"*2
                    assert stored.read("srvros-work/terminal-pwd-status.txt")==b"1\n"
                    if device_checks:
                        assert b"dhcp: 10.0.2.15" in stored.read("srvros-work/device-dhcp.txt")
                        assert "net: provider=usb-ecm" in serial.read_text(errors="replace")
                    shot_bytes=stored.read(shot_path.removeprefix("/work/"))
                    assert shot_bytes[:2]==b"BM" and len(shot_bytes)==54+1280*800*4
                    assert struct.unpack_from("<II",shot_bytes,18)==(1280,800)
                    first_session=re.search(r"workspace-log: directory=(/work/[^ ]+) ",serial.read_text(errors="replace"))[1]
                    if basic_desktop:
                        from desktop_session_workload import verify as verify_session
                        session_proof=verify_session(lambda name:stored.read("srvros-work/"+name),session_run)
                        assert stored.read("srvros-work/replay-active.txt")==b"1\n"
                        metadata=stored.read(".srvros/meta").decode().splitlines()[1:]
                        ids=[int(row.split("\t")[1]) for row in metadata if row]
                        assert all(ids) and len(ids)==len(set(ids)), "Persisted file IDs collide"
                        assert stored.read("srvros-work/collision-a.txt")==b"saved alpha\n"
                        assert stored.read("srvros-work/collision-b.txt")==b"saved beta\n"
                        session_proof.update(picker_reopen=True,home_reentry=True,unique_ids=True,active_replay_refused=True)
                        boot_log=stored.read(first_session.removeprefix("/work/")+"/boot.log")
                        assert b"laptop-probe:" not in boot_log and b"aml-probe:" not in boot_log
                    else:
                        assert b"laptop-probe: unsupported chipset" in stored.read(first_session.removeprefix("/work/")+"/boot.log")
                    assert b"deskd: exited" in stored.read(first_session.removeprefix("/work/")+"/exit.log")
                    if args.audio_check:
                        assert b"workspace-devices: audio_state=2" in stored.read(first_session.removeprefix("/work/")+"/exit.log")
                    if manifest.get("config_version", "").startswith("ram-usb-v59-"):
                        boot_log=stored.read(first_session.removeprefix("/work/")+"/boot.log").decode()
                        assert "smc: state=1" in boot_log and "thermal: state=1" in boot_log
                        assert "aml-probe:" not in boot_log
                    if manifest.get("config_version", "").startswith(("ram-usb-v21-", "ram-usb-v22-", "ram-usb-v58-")):
                        boot_log=stored.read(first_session.removeprefix("/work/")+"/boot.log").decode()
                        assert "smc: state=1" in boot_log and "thermal: state=1" in boot_log
                        if manifest["config_version"].startswith("ram-usb-v21-"):
                            assert re.search(r"aml-probe: completed .*errors=0 denied=0 .*heap_remaining=0",boot_log)
                        else:
                            assert "aml-probe:" not in boot_log
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
                    if args.audio_check:
                        assert "audio: state=2" in text and "tests_started=0 tests_completed=0" in text
                        offset=len(text)
                        send_qmp_text(qmp,"audio test\n",0.08)
                        wait_for(serial,lambda s:"audio: request queued" in s[offset:],process,20)
                        time.sleep(1.5)
                        send_qmp_text(qmp,"audio\n",0.08)
                        text=wait_for(serial,lambda s:"tests_started=1 tests_completed=1" in s[offset:] and s.endswith("srv> "),process,20)
                        offset=len(text)
                        send_qmp_text(qmp,"run /fat/bin/playwav /fat/share/examples/stereo.wav\n",0.08)
                        text=wait_for(serial,lambda s:"playwav: complete bytes=1152000 underruns=0" in s[offset:] and s.endswith("srv> "),process,30)
                        send_qmp_text(qmp,"audio\n",0.08)
                        text=wait_for(serial,lambda s:"audio-pcm: state=4 owner=0" in s[offset:] and s.endswith("srv> "),process,20)
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
            if args.audio_check:
                import wave
                from array import array
                captured=logs/"hardware-grub-tone.wav"
                first_capture=logs/"hardware-grub-tone-first.wav"
                first_capture.write_bytes(captured.read_bytes())
                with wave.open(str(first_capture),"rb") as wav:
                    assert wav.getnchannels()==2 and wav.getsampwidth()==2
                    rate=wav.getframerate();samples=array("h",wav.readframes(wav.getnframes()))
                # The first 650 ms contain the half-second test and a quiet tail.
                assert rate==48000
                tone=samples[:int(rate*.65)*2]
                left=tone[::2];assert left==tone[1::2]
                active=[i for i,v in enumerate(left) if abs(v)>20]
                assert active and .45<(active[-1]-active[0])/rate<.6
                assert max(abs(v) for v in left)==1200 and all(v==0 for v in left[-rate//10:])
                from audio_fixture import verify_channel_capture
                pcm_result=verify_channel_capture(samples,rate,start_frame=int(rate*.65))
                (logs/"hardware-grub-pcm.json").write_text(json.dumps(pcm_result,indent=2)+"\n")
            if args.desktop_check:
                # A new VM has a fresh RAM root. Only the USB workspace survives.
                first_serial=logs/"hardware-grub-first-serial.log"
                first_serial.write_bytes(serial.read_bytes());serial.unlink()
                qmp_path.unlink(missing_ok=True)
                process=subprocess.Popen(command,stdout=errors,stderr=errors)
                try:
                    wait_for(serial,lambda s:"deskhelp: page=laptop-trial" in s and "deskd: opened" in s,process,args.boot_wait)
                    ui=QMP(qmp_path)
                    workspace_preferences(ui,serial,process,logs,restored=True,expanded=manifest.get("config_version", "").startswith(("ram-usb-v20-", "ram-usb-v21-", "ram-usb-v22-", "ram-usb-v58-", "ram-usb-v59-", "ram-usb-v53-", "ram-usb-v56-", "ram-usb-v57-")))
                    if basic_desktop:
                        ui.call("human-monitor-command",{"command-line":"sendkey meta_l-spc"});time.sleep(.3)
                        type_text("terminal new app");ui.key("ret")
                        wait_for(serial,lambda s:"deskterm: ready shell=" in s,process,30)
                        time.sleep(.7)
                        type_text(f"sh /work/srvros-work/refusal-{session_run}.sh; echo $? > replay-complete.txt; exit");ui.key("ret")
                        wait_for(serial,lambda s:"deskterm: shell exited status=0" in s,process,30)
                    for keys in ("f10","end","ret"):
                        ui.call("human-monitor-command",{"command-line":"sendkey "+keys});time.sleep(.3)
                    text=wait_for(serial,lambda s:"desktop-trial: desktop returned=yes USB unmounted=yes" in s and s.endswith("srv> "),process,60)
                    assert not has_fatal_exception(text)
                    stored=verify_workspace(stick,volume,1)
                    assert stored.read("srvros-work/Untitled.txt")==b"Laptop trial AaZz09 saved on USB."
                    assert stored.read(shot_path.removeprefix("/work/"))==shot_bytes
                    if basic_desktop:
                        verify_session(lambda name:stored.read("srvros-work/"+name),session_run)
                        assert stored.read("srvros-work/replay-complete.txt")==b"0\n"
                        from desktop_session_workload import verify_refusal
                        session_proof["refusal_receipt"]=verify_refusal(lambda name:stored.read("srvros-work/"+name),session_run,session_run)
                        session_proof["completed_replay_refused"]=True
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
                    if manifest.get("config_version", "").startswith(("ram-usb-v21-", "ram-usb-v22-", "ram-usb-v58-", "ram-usb-v59-")):
                        for session in report["sessions"]:
                            devices=session["devices"]
                            assert set(devices)=={"audio","thermal","smc","pcm"}
                            assert all(value["monotonic_us"]>0 for value in devices.values())
                            assert devices["thermal"]["thermal_state"]==1 and devices["smc"]["smc_state"]==1
                            if args.audio_check: assert devices["audio"]["audio_state"]==2
                    print(f"desktop diagnostics: two sessions collected and verified: {destination}")
                    (logs/"workspace-final.exfat").write_bytes(volume.read_bytes())
                    ui.stream.close();ui.sock.close()
                finally:
                    process.terminate()
                    try: process.wait(timeout=5)
                    except subprocess.TimeoutExpired: process.kill();process.wait()
    (logs/"hardware-grub-result.json").write_text(json.dumps({
        "status":"PASS","release":release,"manifest_sha256":boot.digest(source/"manifest.json"),
        "desktop":args.desktop_check,"device_checks":device_checks,"desktop_session_workload":session_proof if basic_desktop else None,"audio":args.audio_check,
        "audio_codec_timer":args.audio_codec_timer if args.audio_check else None,
        "console":args.console_check,"kernel":args.kernel_check,
        "workspace_table":args.workspace_table,"responsiveness":args.responsiveness_check,"physical_acceptance":"manual-pending"
    },indent=2)+"\n")
    print(f"hardware-boot-smoke: PASS ({release}); GRUB chainload, RAM filesystem, USB input" + (", case/Caps/repeat/renderbench" if args.console_check else ""))
    if args.kernel_check:
        print("kernel: PASS (nested register preservation, timer/preemption, RAM fsck, cache service windows)")
    if args.responsiveness_check:
        print("responsiveness: PASS (USB input under output load, timer progress, latency samples, Escape, userspace Ctrl-C)")
    if args.desktop_check:
        print("Native desktop trial: checklist, USB keyboard editing, verified GUI save, default USB documents/screenshots, durable session logs, preferences across fresh VM boot and two clean unmounts passed.")
    if args.audio_check: print("Staged audio: PASS HDA initialization, saved device status, 500 ms tone, six-second channel WAV, zero underruns and silent tails")
    print(f"Serial log: {serial}")


if __name__ == "__main__":
    main()
