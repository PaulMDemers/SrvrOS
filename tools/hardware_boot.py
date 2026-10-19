#!/usr/bin/env python3
"""Stage a RAM-backed hardware boot, then install it alongside Ubuntu's EFI loader."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "build/hardware-boot"
ESP = Path("/boot/efi")
BASE = ESP / "EFI/srvros"
SNIPPET = Path("/etc/grub.d/42_srvros")
GRUB_CFG = Path("/boot/grub/grub.cfg")
MARKER = "# Managed by SrvrOS tools/hardware_boot.py"
FILES = ("loader/BOOTX64.EFI", "srvros.elf", "initramfs.tar", "limine.conf", "loader/limine.conf")
CONFIG_VERSION = "ram-usb-v4-console"
DIAGNOSTIC_VERSION = "ram-usb-v4-diagnostic"
CONFIG_VERSIONS = ("ram-usb-v2", "ram-usb-v3-inputdiag", CONFIG_VERSION, DIAGNOSTIC_VERSION)
RELEASE_RE = re.compile(r"[0-9a-f]{16}")
AUTOTEST_RE = re.compile(r"ram-usb-v[5689]-autotest-([0-9a-f]{8})-([0-9a-f]{16})")
WORKSPACE_RE = re.compile(r"ram-usb-v7-workspace-([0-9a-f]{8})")
LAPTOP_PROBE_RE = re.compile(r"ram-usb-v(?:19|23|24)-laptop-probe-([0-9a-f]{8})")
DESKTOP_RE = re.compile(r"ram-usb-v(?:17|18|20|21|22)-desktop-([0-9a-f]{8})")
TRACKPAD_RE = re.compile(r"ram-usb-v(?:10|11|12|13|14|15|16)-trackpad-([0-9a-f]{8})-([0-9a-f]{16})")


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def release_id(hashes, version=CONFIG_VERSION):
    # Configuration version is included so a command-line change creates a new release.
    return hashlib.sha256((version + "\n" + json.dumps(hashes, sort_keys=True)).encode()).hexdigest()[:16]


def limine_config(release, version=CONFIG_VERSION):
    autotest = AUTOTEST_RE.fullmatch(version)
    workspace = WORKSPACE_RE.fullmatch(version)
    trackpad = TRACKPAD_RE.fullmatch(version)
    desktop = DESKTOP_RE.fullmatch(version)
    probe = LAPTOP_PROBE_RE.fullmatch(version)
    if probe:
        cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                   f"srvros.desktop-release={release} srvros.laptop-services=probe-autoreboot")
        if version.startswith("ram-usb-v23-"):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=idle "
                       "srvros.laptop-services=brightness srvros.trackpad=gestures "
                       "srvros.audio=hda srvros.usb-network=ecm srvros.sensors=cpu srvros.smc=read-only")
        if version.startswith("ram-usb-v24-"):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=aml srvros.aml=probe")
    elif desktop:
        cmdline = (f"srvros.storage=ram srvros.workspace-volume={desktop[1]} "
                   "srvros.trackpad=gestures srvros.desktop=trial")
        if version.startswith(("ram-usb-v18-","ram-usb-v20-","ram-usb-v21-","ram-usb-v22-")):
            cmdline += f" srvros.desktop-release={release} srvros.laptop-services=" + ("brightness" if version.startswith(("ram-usb-v20-","ram-usb-v21-","ram-usb-v22-")) else "probe")
        if version.startswith("ram-usb-v21-"): cmdline += " srvros.audio=hda srvros.aml=probe srvros.usb-network=ecm srvros.sensors=cpu srvros.smc=read-only"
        # The optional synchronous AML probe stalled on the physical MacBook
        # before workspace logging/desktop startup. Keep diagnostics opt-in;
        # preserve v21's exact recipe so installed images remain verifiable.
        if version.startswith("ram-usb-v22-"): cmdline += " srvros.audio=hda srvros.usb-network=ecm srvros.sensors=cpu srvros.smc=read-only"
    elif trackpad:
        volume, run = trackpad.groups()
        mode="gestures" if version.startswith(("ram-usb-v12-","ram-usb-v13-","ram-usb-v14-","ram-usb-v15-","ram-usb-v16-")) else "pointer" if version.startswith("ram-usb-v11-") else "capture"
        cmdline = (f"srvros.storage=ram srvros.workspace-volume={volume} srvros.trackpad={mode} "
                   f"srvros.trackpad-run={run} srvros.trackpad-release={release}")
        if version.startswith(("ram-usb-v13-","ram-usb-v14-","ram-usb-v15-","ram-usb-v16-")): cmdline += f" srvros.desktop-io={run}"
    elif autotest:
        volume, run = autotest.groups()
        cmdline = (f"srvros.storage=ram srvros.autotest-volume={volume} "
                   f"srvros.autotest-run={run} srvros.autotest-release={release}")
        if version.startswith('ram-usb-v9-'):
            cmdline += ' srvros.autotest-repeat=5'
    elif workspace:
        cmdline = f"srvros.storage=ram srvros.workspace-volume={workspace[1]}"
    elif version == CONFIG_VERSION:
        cmdline = "srvros.storage=ram"
    elif version == DIAGNOSTIC_VERSION:
        cmdline = "srvros.log=debug srvros.storage=ram srvros.inputdiag=1 srvros.inputprobe=1"
    else:
        cmdline = "srvros.log=debug srvros.storage=ram"
        if version == "ram-usb-v3-inputdiag":
            cmdline += " srvros.inputdiag=1"
    return f"""timeout: 0
serial: yes

/SrvrOS hardware test {release}
    protocol: limine
    path: boot():/EFI/srvros/{release}/srvros.elf
    cmdline: {cmdline}
    module_path: boot():/EFI/srvros/{release}/initramfs.tar
    module_string: initramfs
"""


def verify(directory):
    manifest = json.loads((directory / "manifest.json").read_text())
    hashes = manifest["sha256"]
    if set(hashes) != set(FILES):
        raise ValueError("Unexpected files in boot manifest")
    for name in FILES:
        path = directory / name
        if path.is_symlink() or not path.is_file() or digest(path) != hashes[name]:
            raise ValueError(f"Boot artifact failed verification: {path}")
    payload = {name: hashes[name] for name in FILES if not name.endswith("limine.conf")}
    # Older installs have no config_version field. Verify against their original
    # recipe so a config update can retain the working previous release.
    version = manifest.get("config_version")
    if version is None:
        version = next((v for v in CONFIG_VERSIONS if release_id(payload, v) == manifest["release"]), None)
    if version not in CONFIG_VERSIONS and not (isinstance(version, str) and
            (AUTOTEST_RE.fullmatch(version) or WORKSPACE_RE.fullmatch(version) or TRACKPAD_RE.fullmatch(version) or DESKTOP_RE.fullmatch(version) or LAPTOP_PROBE_RE.fullmatch(version))):
        raise ValueError("Unknown boot configuration version")
    release = release_id(payload, version)
    if manifest["release"] != release or any(
            (directory / name).read_text() != limine_config(release, version)
            for name in ("limine.conf", "loader/limine.conf")):
        raise ValueError("Boot configuration or release identity mismatch")
    return manifest


def stage(diagnostic=False, autotest_volume=None, run_id=None, workspace_volume=None, repeat=1, trackpad_run=None, pointer=False, gestures=False, desktop_io=False, desktop_volume=None, laptop_probe_volume=None, idle_capture=False, aml_capture=False):
    if (idle_capture or aml_capture) and laptop_probe_volume is None:
        raise ValueError("Idle/AML capture requires a laptop probe volume")
    if idle_capture and aml_capture:
        raise ValueError("Idle and AML captures are separate profiles")
    gestures=gestures or desktop_io
    pointer=pointer or gestures
    if pointer and trackpad_run is None:
        raise ValueError("Pointer trial requires a trackpad run")
    if repeat not in (1, 5) or (repeat != 1 and (autotest_volume is None or run_id is None)):
        raise ValueError('Invalid automated-test repeat count')
    version = DIAGNOSTIC_VERSION if diagnostic else CONFIG_VERSION
    if workspace_volume is not None:
        version = f"ram-usb-v7-workspace-{workspace_volume}"
        if diagnostic or autotest_volume is not None or run_id is not None or not WORKSPACE_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting workspace profile")
    if autotest_volume is not None or run_id is not None:
        version = f"ram-usb-v{9 if repeat == 5 else 8}-autotest-{autotest_volume}-{run_id}"
        if diagnostic or not AUTOTEST_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting automated-test profile")
    if trackpad_run is not None:
        version = f"ram-usb-v{16 if desktop_io else 12 if gestures else 11 if pointer else 10}-trackpad-{workspace_volume}-{trackpad_run}"
        if diagnostic or autotest_volume is not None or run_id is not None or repeat != 1 or not TRACKPAD_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting trackpad profile")
    if desktop_volume is not None:
        version = f"ram-usb-v22-desktop-{desktop_volume}"
        if diagnostic or autotest_volume is not None or run_id is not None or workspace_volume is not None or trackpad_run is not None or pointer or repeat != 1 or not DESKTOP_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting native desktop profile")
    if laptop_probe_volume is not None:
        version=f"ram-usb-v{24 if aml_capture else 23 if idle_capture else 19}-laptop-probe-{laptop_probe_volume}"
        if diagnostic or autotest_volume is not None or run_id is not None or workspace_volume is not None or trackpad_run is not None or pointer or repeat != 1 or desktop_volume is not None or not LAPTOP_PROBE_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting automatic laptop probe profile")
    sources = {
        "loader/BOOTX64.EFI": ROOT / "build/limine-binary/BOOTX64.EFI",
        "srvros.elf": ROOT / "build/kernel/srvros.elf",
        "initramfs.tar": ROOT / "build/initramfs.tar",
    }
    STAGE.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="stage-", dir=STAGE) as tmp:
        target = Path(tmp)
        (target / "loader").mkdir()
        for name, source in sources.items():
            shutil.copyfile(source, target / name)
        hashes = {name: digest(target / name) for name in sources}
        release = release_id(hashes, version)
        (target / "limine.conf").write_text(limine_config(release, version))
        # GRUB splits the EFI file path into directory and filename nodes.
        # Limine 11 reads only the first node, so it searches one level up.
        # Keep both locations for GRUB and firmware with a single path node.
        shutil.copyfile(target / "limine.conf", target / "loader/limine.conf")
        hashes["limine.conf"] = digest(target / "limine.conf")
        hashes["loader/limine.conf"] = hashes["limine.conf"]
        manifest = {"release": release, "sha256": hashes, "config_version": version,
                    "git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                    "git_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT))}
        (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        verify(target)
        destination = STAGE / release
        if destination.exists():
            verify(destination)
        else:
            shutil.copytree(target, destination)
    (STAGE / "latest").write_text(release + "\n")
    print(f"Staged {release}: {destination}")
    print("Install with: sudo python3 tools/hardware_boot.py install")
    return release


def grub_config(uuid, current, previous=None):
    if not re.fullmatch(r"[0-9A-Fa-f-]+", uuid):
        raise ValueError("Unexpected EFI filesystem UUID")
    lines = ["set timeout_style=menu", "set timeout=5"]
    for role, release in (("current", current), ("previous", previous)):
        if release is None:
            continue
        if not RELEASE_RE.fullmatch(release):
            raise ValueError("Invalid release identifier")
        lines += [f"menuentry 'SrvrOS ({role}, RAM filesystem) - {release}' --id srvros-{role} {{",
                  "    insmod part_gpt", "    insmod fat", "    insmod chain",
                  f"    search --no-floppy --fs-uuid --set=root {uuid}",
                  f"    chainloader /EFI/srvros/{release}/loader/BOOTX64.EFI", "}"]
    return "\n".join(lines) + "\n"


def host_uuid():
    if not Path("/sys/firmware/efi").is_dir():
        raise RuntimeError("This installer requires a Linux UEFI boot")
    mounts = json.loads(subprocess.check_output(
        ["findmnt", "-J", "-M", str(ESP), "-o", "TARGET,FSTYPE,UUID,OPTIONS"], text=True))["filesystems"]
    if len(mounts) != 1 or mounts[0]["fstype"] != "vfat" or "rw" not in mounts[0]["options"].split(","):
        raise RuntimeError("/boot/efi must be a separately mounted, writable FAT EFI partition")
    # A Mac without Secure Boot has no SecureBoot variable. Other hosts must expose it.
    variables = list(Path("/sys/firmware/efi/efivars").glob("SecureBoot-*"))
    if variables:
        data = variables[0].read_bytes()
        if len(data) != 5 or data[4] != 0:
            raise RuntimeError("Secure Boot is enabled or unreadable; unsigned SrvrOS cannot be installed by this helper")
    else:
        state = subprocess.run(["mokutil", "--sb-state"], text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
        if "doesn't support Secure Boot" not in state:
            raise RuntimeError("Cannot establish that Secure Boot is disabled")
    return mounts[0]["uuid"]


def atomic_write(path, data, mode):
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        temp = Path(stream.name)
        try:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
            os.chmod(temp, mode)
            os.replace(temp, path)
        finally:
            temp.unlink(missing_ok=True)


def install(release=None, keep_previous=False):
    if os.geteuid() != 0:
        raise RuntimeError("Installation needs root: sudo python3 tools/hardware_boot.py install")
    release = release or (STAGE / "latest").read_text().strip()
    if not RELEASE_RE.fullmatch(release):
        raise ValueError("Invalid staged release")
    source = STAGE / release
    verify(source)
    uuid = host_uuid()
    for program in ("update-grub", "grub-script-check"):
        if shutil.which(program) is None:
            raise RuntimeError(f"Missing host command: {program}")
    state_path = BASE / "state.json"
    old = {}
    if BASE.exists():
        if not state_path.is_file():
            raise RuntimeError(f"Refusing to adopt unmanaged directory {BASE}")
        old = json.loads(state_path.read_text())
        if old.get("owner") != MARKER:
            raise RuntimeError("Unrecognized boot installation state")
        for key in ("current", "previous"):
            if old.get(key) and not RELEASE_RE.fullmatch(old[key]):
                raise ValueError("Invalid installed release identifier")
        if old.get("current"):
            verify(BASE / old["current"])
    if SNIPPET.exists() and MARKER not in SNIPPET.read_text().splitlines():
        raise RuntimeError(f"Refusing to replace unmanaged {SNIPPET}")
    previous = old.get("current") if old.get("current") != release else old.get("previous")
    if keep_previous:
        previous = old.get("previous")
        if not previous or previous == release:
            raise ValueError("No distinct previous release to preserve")
        verify(BASE / previous)
    config = grub_config(uuid, release, previous)
    subprocess.run(["grub-script-check"], input=config, text=True, check=True)
    destination = BASE / release
    needed = sum((source / name).stat().st_size for name in (*FILES, "manifest.json"))
    if not destination.exists() and shutil.disk_usage(ESP).free < needed + 32 * 1024 * 1024:
        raise RuntimeError("EFI partition needs space for a new release plus a 32 MiB margin")
    BASE.mkdir(parents=True, exist_ok=True)
    # Record ownership before copying, so an interrupted first install is recoverable.
    if not state_path.exists():
        atomic_write(state_path, (json.dumps({"owner": MARKER, "current": None}) + "\n").encode(), 0o644)
    if destination.exists():
        verify(destination)
    else:
        with tempfile.TemporaryDirectory(prefix="upload-", dir=BASE) as tmp:
            payload = Path(tmp) / "payload"
            shutil.copytree(source, payload)
            verify(payload)
            os.sync()
            payload.rename(destination)
    # Keep Ubuntu's existing GRUB_DEFAULT and firmware boot order. Only add entries.
    script = "#!/bin/sh\n" + MARKER + "\ncat <<'SRVROS_GRUB'\n" + config + "SRVROS_GRUB\n"
    backups = {path: (path.read_bytes(), path.stat().st_mode & 0o777) if path.exists() else None
               for path in (SNIPPET, GRUB_CFG, state_path)}
    try:
        atomic_write(SNIPPET, script.encode(), 0o755)
        subprocess.run(["update-grub"], check=True)
        subprocess.run(["grub-script-check", str(GRUB_CFG)], check=True)
        atomic_write(state_path, (json.dumps({"owner": MARKER, "current": release,
                     "previous": previous}, indent=2) + "\n").encode(), 0o644)
    except BaseException:
        for path, backup in backups.items():
            if backup is None:
                path.unlink(missing_ok=True)
            else:
                atomic_write(path, *backup)
        raise
    os.sync()
    # Remove only the obsolete release previously recorded by this installer.
    obsolete = old.get("previous")
    if obsolete and obsolete not in (release, previous):
        verify(BASE / obsolete)
        shutil.rmtree(BASE / obsolete)
    print(f"Installed {release}. Ubuntu's default and EFI boot order are unchanged.")
    print("Select SrvrOS in the five-second GRUB menu, or: sudo grub-reboot srvros-current")
    print("No reboot has been requested.")
    if AUTOTEST_RE.fullmatch(verify(source).get("config_version", "")):
        print("Automated test profile: matching USB request enables log writes and guest reboot.")
    elif TRACKPAD_RE.fullmatch(verify(source).get("config_version", "")):
        print("Trackpad capture: interactive gestures/keyboard, UUID-selected workspace report, then halt.")
    elif LAPTOP_PROBE_RE.fullmatch(verify(source).get("config_version", "")):
        print("Automatic laptop diagnostics: USB snapshots, flush/unmount, reboot on success; halt on failure.")
    elif DESKTOP_RE.fullmatch(verify(source).get("config_version", "")):
        print("Native desktop trial: on-screen checklist, prepared USB workspace, no timed exit or automatic reboot.")
    elif WORKSPACE_RE.fullmatch(verify(source).get("config_version", "")):
        print("Workspace profile: prepared UUID-selected USB volume mounted at /work; boot filesystem stays in RAM.")
    else:
        print("Guest root filesystem/log changes are held in RAM.")


def compact_volume_uuid(value):
    if not re.fullmatch(r"(?:[0-9a-fA-F]{8}|[0-9a-fA-F]{4}-[0-9a-fA-F]{4})",value):
        raise argparse.ArgumentTypeError("Expected an eight-digit exFAT UUID, optionally hyphenated")
    return value.replace("-", "").lower()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("stage", "install"))
    parser.add_argument("--diagnostic", action="store_true", help="stage verbose input-probe boot")
    parser.add_argument("--release", help="install this exact staged release")
    parser.add_argument("--keep-previous", action="store_true", help="retain the verified existing recovery entry during install")
    parser.add_argument("--desktop-volume", type=compact_volume_uuid, help="stage an interactive native desktop trial for this compact USB UUID")
    parser.add_argument("--laptop-probe-volume", type=compact_volume_uuid, help="stage unattended laptop diagnostics on this prepared USB UUID")
    capture=parser.add_mutually_exclusive_group()
    capture.add_argument("--aml-capture",action="store_true",help="run the isolated snapshot-only AML diagnostic with workspace logging; return only after clean completion")
    capture.add_argument("--idle-capture", action="store_true", help="sample idle scheduling and cached devices for 60 seconds, save, unmount and return")
    args = parser.parse_args()
    if (args.idle_capture or args.aml_capture) and (args.action != "stage" or not args.laptop_probe_volume):
        parser.error("capture requires stage --laptop-probe-volume")
    if args.laptop_probe_volume and args.action != "stage":
        parser.error("--laptop-probe-volume applies only to stage")
    if args.keep_previous and args.action != "install":
        parser.error("--keep-previous applies only to install")
    if args.release and args.action != "install":
        parser.error("--release applies only to install")
    if args.desktop_volume and args.action != "stage":
        parser.error("--desktop-volume applies only to stage")
    if args.diagnostic and args.action != "stage":
        parser.error("--diagnostic applies only to stage")
    try:
        if args.action == "stage":
            stage(args.diagnostic, desktop_volume=args.desktop_volume, laptop_probe_volume=args.laptop_probe_volume, idle_capture=args.idle_capture, aml_capture=args.aml_capture)
        else:
            install(args.release, keep_previous=args.keep_previous)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"hardware-boot: {error}\n")


if __name__ == "__main__":
    main()
