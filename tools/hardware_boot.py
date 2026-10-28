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
LAPTOP_PROBE_RE = re.compile(r"ram-usb-v(?:1-ap-owner|1-ap-c3|1-ap-package|1-posix|1-storage-wait|1-mwait|1-mwait-c3|19|23|24|25|26|27|28|31|32|33|34|35|36|38|40|41|42|43|44|45|46|47|48|49|51|52|60|61|62|63|64|65|66|67|68|69)-laptop-probe-([0-9a-f]{8})(-auto-return)?")
DESKTOP_RE = re.compile(r"ram-usb-v(?:17|18|20|21|22|29|30|31|36|37|53|54|55|56|57|58|59|70|71|72|73|74|75|76|77|78|79|80|81|82)-desktop-([0-9a-f]{8})")
TRACKPAD_RE = re.compile(r"ram-usb-v(?:10|11|12|13|14|15|16|50)-trackpad-([0-9a-f]{8})-([0-9a-f]{16})")


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
        if version.startswith(("ram-usb-v23-","ram-usb-v25-","ram-usb-v26-")):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=idle "
                       "srvros.laptop-services=brightness srvros.trackpad=gestures "
                       "srvros.audio=hda srvros.usb-network=ecm srvros.sensors=cpu srvros.smc=read-only")
        if version.startswith(("ram-usb-v25-","ram-usb-v26-")): cmdline=cmdline.replace("srvros.capture=idle","srvros.capture=desktop-idle")
        if version.startswith("ram-usb-v26-"): cmdline += " srvros.power=read-only"
        if version.startswith("ram-usb-v24-"):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=aml srvros.aml=probe")
        if version.startswith(("ram-usb-v31-","ram-usb-v32-","ram-usb-v33-","ram-usb-v34-","ram-usb-v36-","ram-usb-v38-","ram-usb-v40-","ram-usb-v41-","ram-usb-v42-","ram-usb-v43-","ram-usb-v44-","ram-usb-v45-","ram-usb-v46-","ram-usb-v47-","ram-usb-v48-","ram-usb-v49-","ram-usb-v51-","ram-usb-v52-","ram-usb-v61-","ram-usb-v62-","ram-usb-v63-","ram-usb-v64-","ram-usb-v65-","ram-usb-v66-","ram-usb-v67-","ram-usb-v68-","ram-usb-v69-")):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=battery")
            if version.startswith("ram-usb-v69-"): cmdline += "-home-work srvros.trackpad=gestures"
            if version.startswith("ram-usb-v63-"): cmdline += "-smbus-transitions"
            if version.startswith("ram-usb-v32-"): cmdline += "-transitions"
            if version.startswith("ram-usb-v33-"): cmdline += "-service"
            if version.startswith("ram-usb-v34-"): cmdline += "-usb-read"
            if version.startswith("ram-usb-v36-"): cmdline += "-usb-write"
            if version.startswith("ram-usb-v38-"): cmdline += "-filesystem"
            if version.startswith("ram-usb-v40-"): cmdline += "-desktop srvros.trackpad=gestures"
            if version.startswith("ram-usb-v41-"): cmdline += "-text srvros.trackpad=gestures"
            if version.startswith(("ram-usb-v52-","ram-usb-v68-")): cmdline += "-home-transitions srvros.trackpad=gestures"
            if version.startswith(("ram-usb-v51-","ram-usb-v67-")): cmdline += "-home-desktop srvros.trackpad=gestures"
            if version.startswith(("ram-usb-v49-","ram-usb-v66-")): cmdline += "-home-session srvros.trackpad=gestures"
            if version.startswith(("ram-usb-v48-","ram-usb-v65-")): cmdline += "-home"
            if version.startswith(("ram-usb-v47-","ram-usb-v64-")): cmdline += "-smbus-files srvros.trackpad=gestures"
            if version.startswith(("ram-usb-v46-","ram-usb-v62-")): cmdline += "-smbus"
            if version.startswith("ram-usb-v45-"): cmdline += "-query"
            if version.startswith("ram-usb-v44-"): cmdline += "-controls srvros.trackpad=gestures"
            if version.startswith("ram-usb-v43-"): cmdline += "-app-files srvros.trackpad=gestures"
            if version.startswith("ram-usb-v42-"): cmdline += "-load srvros.trackpad=gestures"
        if version.startswith(("ram-usb-v35-","ram-usb-v1-mwait-","ram-usb-v1-ap-c3-","ram-usb-v1-ap-package-")):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=cpu-load "
                       "srvros.sensors=cpu srvros.smc=read-only srvros.power=read-only")
        if version.startswith("ram-usb-v1-mwait-c3-"): cmdline += " srvros.idle=mwait-c3-test"
        elif version.startswith("ram-usb-v1-mwait-"): cmdline += " srvros.idle=mwait-wakeup-test"
        if version.startswith(("ram-usb-v1-ap-c3-","ram-usb-v1-ap-package-")): cmdline += " srvros.idle=mwait-c3-test srvros.ap=c3-test"
        if version.startswith("ram-usb-v1-ap-package-"): cmdline += " srvros.ap-package=read-only"
        if version.startswith("ram-usb-v1-ap-owner-"):
            cmdline=(f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                     f"srvros.desktop-release={release} srvros.capture=ap-ownership")
        if version.startswith("ram-usb-v60-"): cmdline += " srvros.battery-preflight=air61"
        if version.startswith("ram-usb-v1-storage-wait-"): cmdline += " srvros.capture=storage-wait"
        if version.startswith("ram-usb-v1-posix-"):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=posix srvros.trackpad=gestures")
        if probe[2]: cmdline += " srvros.capture-failure=reboot"
        if version.startswith(("ram-usb-v27-","ram-usb-v28-")): cmdline += " srvros.capture=keyboard"
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
        if version.startswith(("ram-usb-v29-","ram-usb-v30-","ram-usb-v31-","ram-usb-v36-","ram-usb-v37-")):
            cmdline += f" srvros.desktop-release={release} srvros.desktop-scale=2 srvros.laptop-services=" + ("brightness" if version.startswith(("ram-usb-v30-","ram-usb-v31-","ram-usb-v36-","ram-usb-v37-")) else "probe")
        if version.startswith(("ram-usb-v31-","ram-usb-v36-","ram-usb-v37-")): cmdline += " srvros.audio=hda"
        if version.startswith(("ram-usb-v36-","ram-usb-v37-")): cmdline += " srvros.sensors=cpu srvros.smc=read-only"
        if version.startswith("ram-usb-v37-"): cmdline += " srvros.usb-network=ecm"
        if version.startswith("ram-usb-v53-"):
            cmdline += f" srvros.desktop-release={release}"
        if version.startswith(("ram-usb-v56-","ram-usb-v57-","ram-usb-v59-")):
            cmdline += f" srvros.desktop-release={release} srvros.laptop-services=brightness"
        if version.startswith(("ram-usb-v57-","ram-usb-v59-")): cmdline += " srvros.audio=hda"
        if version.startswith("ram-usb-v59-"): cmdline += " srvros.sensors=cpu srvros.smc=read-only"
        if version.startswith(("ram-usb-v54-","ram-usb-v55-","ram-usb-v70-","ram-usb-v71-","ram-usb-v72-","ram-usb-v73-","ram-usb-v74-","ram-usb-v75-","ram-usb-v76-","ram-usb-v77-","ram-usb-v78-","ram-usb-v79-","ram-usb-v80-","ram-usb-v81-","ram-usb-v82-")):
            cmdline = cmdline.replace("srvros.desktop=trial", "srvros.desktop=session")
            cmdline += f" srvros.desktop-release={release} srvros.battery=on"
        if version.startswith(("ram-usb-v55-","ram-usb-v73-","ram-usb-v75-","ram-usb-v76-")):
            cmdline += " srvros.laptop-services=brightness srvros.audio=hda srvros.sensors=cpu srvros.smc=read-only srvros.usb-network=ecm"
        if version.startswith(("ram-usb-v71-","ram-usb-v72-","ram-usb-v74-","ram-usb-v77-","ram-usb-v78-","ram-usb-v79-","ram-usb-v80-","ram-usb-v81-","ram-usb-v82-")):
            cmdline += " srvros.laptop-services=brightness srvros.audio=hda srvros.sensors=cpu srvros.smc=read-only"
        if version.startswith(("ram-usb-v78-","ram-usb-v82-")): cmdline += " srvros.desktop-power=on srvros.power=read-only"
        if version.startswith(("ram-usb-v72-","ram-usb-v79-")): cmdline += " srvros.desktop-reopen-test=on"
        if version.startswith("ram-usb-v76-"): cmdline += " srvros.desktop-guide=window-navigation"
        if version.startswith("ram-usb-v75-"): cmdline += " srvros.desktop-guide=application-lifecycle"
        if version.startswith(("ram-usb-v74-","ram-usb-v80-")): cmdline += " srvros.desktop-audio-io=on"
        if version.startswith(("ram-usb-v77-","ram-usb-v81-")): cmdline += " srvros.desktop-audio-io=attended"
        if version.startswith("ram-usb-v73-"): cmdline += " srvros.desktop-capacity-test=on"
        if version.startswith("ram-usb-v58-"):
            cmdline += f" srvros.desktop-release={release} srvros.desktop-check=devices"
            cmdline += " srvros.laptop-services=brightness srvros.audio=hda srvros.usb-network=ecm srvros.sensors=cpu srvros.smc=read-only"
    elif trackpad:
        volume, run = trackpad.groups()
        mode="gestures" if version.startswith(("ram-usb-v50-","ram-usb-v12-","ram-usb-v13-","ram-usb-v14-","ram-usb-v15-","ram-usb-v16-")) else "pointer" if version.startswith("ram-usb-v11-") else "capture"
        cmdline = (f"srvros.storage=ram srvros.workspace-volume={volume} srvros.trackpad={mode} "
                   f"srvros.trackpad-run={run} srvros.trackpad-release={release}")
        if version.startswith(("ram-usb-v13-","ram-usb-v14-","ram-usb-v15-","ram-usb-v16-")): cmdline += f" srvros.desktop-io={run}"
        if version.startswith("ram-usb-v50-"): cmdline += f" srvros.trackpad-policy={run}"
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
    resolution="    resolution: 2880x1800\n" if desktop and version.startswith(("ram-usb-v29-","ram-usb-v30-","ram-usb-v31-","ram-usb-v36-","ram-usb-v37-")) else ""
    return f"""timeout: 0
serial: yes

/SrvrOS hardware test {release}
{resolution}    protocol: limine
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


def stage(diagnostic=False, autotest_volume=None, run_id=None, workspace_volume=None, repeat=1, trackpad_run=None, pointer=False, gestures=False, desktop_io=False, desktop_volume=None, laptop_probe_volume=None, idle_capture=False, aml_capture=False, desktop_idle_capture=False, power_telemetry=False, battery_capture=False, battery_transitions=False, battery_service=False, hold_on_failure=False, battery_usb_read=False, battery_usb_write=False, battery_filesystem=False, keyboard_capture=False, cpu_load_capture=False, desktop_scale=1, desktop_brightness=False, desktop_audio=False, desktop_sensors=False, desktop_network=False, battery_desktop=False, battery_text=False, battery_load=False, battery_app_files=False, battery_controls=False, battery_query=False, battery_smbus=False, battery_smbus_files=False, battery_home=False, battery_home_session=False, battery_home_desktop=False, battery_home_transitions=False, battery_home_work=False, tap_typing=False, desktop_basic=False, desktop_battery=False, desktop_services=False, desktop_device_checks=False, battery_preflight=False, battery_air61=False, storage_wait_capture=False, desktop_reopen_test=False, posix_capture=False, desktop_capacity_test=False, desktop_lifecycle_trial=False, desktop_audio_io_test=False, desktop_audio_io_attended=False, desktop_navigation_trial=False, desktop_power_test=False, mwait_wakeup_test=False, mwait_deep_test=False, ap_ownership_test=False, ap_coordinated_c3=False, ap_package_audit=False):
    if ap_package_audit and not ap_coordinated_c3:
        raise ValueError("Package audit requires explicit coordinated C3")
    if ap_coordinated_c3:
        allowed={"ap_package_audit","ap_coordinated_c3","laptop_probe_volume","cpu_load_capture","hold_on_failure","repeat","desktop_scale"}
        if not laptop_probe_volume or not cpu_load_capture or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("Coordinated C3 requires its own explicit CPU-load workspace profile")
    if ap_ownership_test:
        allowed={"ap_ownership_test","laptop_probe_volume","hold_on_failure","repeat","desktop_scale"}
        if not laptop_probe_volume or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("AP ownership requires its own explicit workspace profile")
    if mwait_deep_test and mwait_wakeup_test:
        raise ValueError("Select one MWAIT trial")
    if (mwait_wakeup_test or mwait_deep_test) and not cpu_load_capture:
        raise ValueError("MWAIT wakeup trial requires the explicit CPU-load capture")
    if desktop_navigation_trial:
        allowed={"desktop_volume","desktop_battery","desktop_services","desktop_navigation_trial","repeat","desktop_scale"}
        if not desktop_volume or not desktop_battery or not desktop_services or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("Navigation trial requires its own combined Air7,2 desktop profile")
    if desktop_lifecycle_trial:
        allowed={"desktop_volume","desktop_battery","desktop_services","desktop_lifecycle_trial","repeat","desktop_scale"}
        if not desktop_volume or not desktop_battery or not desktop_services or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("Lifecycle trial requires its own combined Air7,2 desktop profile")
    if desktop_capacity_test:
        allowed={"desktop_volume","desktop_battery","desktop_services","desktop_capacity_test","repeat","desktop_scale"}
        if not desktop_volume or not desktop_battery or not desktop_services or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("Capacity diagnostic requires its own combined Air7,2 desktop profile")
    if posix_capture:
        allowed={"laptop_probe_volume", "posix_capture", "repeat", "desktop_scale"}
        if not laptop_probe_volume or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("POSIX capture requires its own automatic laptop probe profile")
    if storage_wait_capture:
        allowed={"laptop_probe_volume", "storage_wait_capture", "repeat", "desktop_scale"}
        if not laptop_probe_volume or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("Storage wait comparison requires its own automatic laptop probe profile")
    if any((desktop_power_test,desktop_audio_io_attended,desktop_audio_io_test,desktop_reopen_test)):
        allowed={"desktop_volume","desktop_battery","desktop_services","desktop_power_test",
                 "desktop_audio_io_attended","desktop_audio_io_test","desktop_reopen_test",
                 "battery_air61","repeat","desktop_scale"}
        if (not desktop_volume or not desktop_battery or not desktop_services or repeat!=1 or desktop_scale!=1
            or sum(map(bool,(desktop_power_test,desktop_audio_io_attended,desktop_audio_io_test,desktop_reopen_test)))!=1
            or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed")):
            raise ValueError("Desktop diagnostic requires its own unscaled combined Air battery profile")
    if battery_home_work and not battery_air61:raise ValueError("Attended Home work currently requires Air6,1 profile")
    if battery_air61 and desktop_battery:
        allowed={"desktop_volume", "desktop_battery", "desktop_services", "desktop_reopen_test", "desktop_audio_io_test", "desktop_audio_io_attended", "desktop_power_test", "battery_air61", "repeat", "desktop_scale"}
        if not desktop_volume or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("Air6,1 ordinary battery requires its own unscaled desktop profile")
    elif battery_air61:
        allowed={"laptop_probe_volume", "battery_air61", "battery_smbus", "battery_transitions", "battery_smbus_files", "battery_home", "battery_home_session", "battery_home_desktop", "battery_home_transitions", "battery_home_work", "repeat", "desktop_scale"}
        if not laptop_probe_volume or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("Air6,1 battery requires its own automatic sampling or bounded SMBus profile")
    if battery_preflight:
        allowed={"laptop_probe_volume", "battery_preflight", "hold_on_failure", "repeat", "desktop_scale"}
        if not laptop_probe_volume or repeat!=1 or desktop_scale!=1 or any(v for k,v in locals().copy().items() if k not in allowed and k!="allowed"):
            raise ValueError("Battery preflight requires its own unscaled laptop-probe profile")
    if (battery_usb_read or battery_usb_write or battery_filesystem or battery_desktop or battery_text or battery_load or battery_app_files or battery_controls or battery_query or battery_smbus or battery_smbus_files or battery_home or battery_home_session or battery_home_desktop or battery_home_transitions or battery_home_work) and hold_on_failure:
        raise ValueError("USB overlap requires automatic checkpoint/return")
    if hold_on_failure and laptop_probe_volume is None:
        raise ValueError("Hold-on-failure requires a laptop probe profile")
    if battery_transitions and battery_capture:
        raise ValueError("Battery sample and transition captures are separate profiles")
    if desktop_device_checks and (desktop_volume is None or desktop_scale != 1 or any((desktop_basic, desktop_battery, desktop_services, desktop_brightness, desktop_audio, desktop_sensors, desktop_network))):
        raise ValueError("Device checks require their own unscaled desktop volume profile")
    if desktop_services and not desktop_battery:
        raise ValueError("Laptop services require the explicit ordinary battery desktop profile")
    if desktop_battery and (desktop_volume is None or desktop_basic or desktop_scale != 1 or desktop_brightness or desktop_audio or desktop_sensors or desktop_network):
        raise ValueError("Battery desktop requires its own unscaled Home profile")
    if desktop_basic and (desktop_volume is None or desktop_scale != 1 or desktop_network):
        raise ValueError("Basic desktop requires an unscaled volume; only explicit brightness/audio/sensors are optional")
    if desktop_network and not desktop_sensors:
        raise ValueError("desktop network requires the scaled sensor/audio profile")
    if desktop_sensors and not desktop_audio:
        raise ValueError("desktop sensors require the explicit audio/brightness profile")
    if desktop_audio and not desktop_brightness:
        raise ValueError("desktop audio requires explicit brightness profile")
    if desktop_brightness and ((desktop_scale!=2 and not desktop_basic) or desktop_volume is None):
        raise ValueError("desktop brightness requires a scaled desktop volume")
    if desktop_scale not in (1,2) or (desktop_scale!=1 and desktop_volume is None):
        raise ValueError("Desktop scale requires desktop volume and scale 1 or 2")
    if power_telemetry and not desktop_idle_capture:
        raise ValueError("Power telemetry requires the explicit desktop idle capture")
    if (idle_capture or aml_capture or desktop_idle_capture or battery_capture or battery_transitions or battery_service or battery_usb_read or battery_usb_write or battery_filesystem or battery_desktop or battery_text or battery_load or battery_app_files or battery_controls or battery_query or battery_smbus or battery_smbus_files or battery_home or battery_home_session or battery_home_desktop or battery_home_transitions or battery_home_work or keyboard_capture or cpu_load_capture) and laptop_probe_volume is None:
        raise ValueError("Idle/AML capture requires a laptop probe volume")
    if sum(map(bool,(idle_capture,aml_capture,desktop_idle_capture,battery_capture,battery_transitions,battery_service,battery_usb_read,battery_usb_write,battery_filesystem,battery_desktop,battery_text,battery_load,battery_app_files,battery_controls,battery_query,battery_smbus,battery_smbus_files,battery_home,battery_home_session,battery_home_desktop,battery_home_transitions,battery_home_work,keyboard_capture,cpu_load_capture)))>1:
        raise ValueError("Idle and AML captures are separate profiles")
    if tap_typing and (desktop_io or trackpad_run is None):
        raise ValueError("Tap/typing trial requires its own trackpad run")
    gestures=gestures or desktop_io or tap_typing
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
        version = f"ram-usb-v{50 if tap_typing else 16 if desktop_io else 12 if gestures else 11 if pointer else 10}-trackpad-{workspace_volume}-{trackpad_run}"
        if diagnostic or autotest_volume is not None or run_id is not None or repeat != 1 or not TRACKPAD_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting trackpad profile")
    if desktop_volume is not None:
        version = f"ram-usb-v{(78 if battery_air61 else 82) if desktop_power_test else (77 if battery_air61 else 81) if desktop_audio_io_attended else 76 if desktop_navigation_trial else 75 if desktop_lifecycle_trial else (74 if battery_air61 else 80) if desktop_audio_io_test else 73 if desktop_capacity_test else (72 if battery_air61 else 79) if desktop_reopen_test else 71 if battery_air61 and desktop_services else 70 if battery_air61 and desktop_battery else 58 if desktop_device_checks else 55 if desktop_services else 54 if desktop_battery else 59 if desktop_basic and desktop_sensors else 57 if desktop_basic and desktop_audio else 56 if desktop_basic and desktop_brightness else 53 if desktop_basic else 37 if desktop_network else 36 if desktop_sensors else 31 if desktop_audio else 30 if desktop_brightness else 29 if desktop_scale==2 else 22}-desktop-{desktop_volume}"
        if diagnostic or autotest_volume is not None or run_id is not None or workspace_volume is not None or trackpad_run is not None or pointer or repeat != 1 or not DESKTOP_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting native desktop profile")
    if laptop_probe_volume is not None:
        version=f"ram-usb-v{69 if battery_air61 and battery_home_work else 68 if battery_air61 and battery_home_transitions else 67 if battery_air61 and battery_home_desktop else 66 if battery_air61 and battery_home_session else 65 if battery_air61 and battery_home else 64 if battery_air61 and battery_smbus_files else 63 if battery_air61 and battery_transitions else 62 if battery_air61 and battery_smbus else 61 if battery_air61 else 60 if battery_preflight else 52 if battery_home_transitions else 51 if battery_home_desktop else 49 if battery_home_session else 48 if battery_home else 47 if battery_smbus_files else 46 if battery_smbus else 45 if battery_query else 44 if battery_controls else 43 if battery_app_files else 42 if battery_load else 41 if battery_text else 40 if battery_desktop else 38 if battery_filesystem else 35 if cpu_load_capture else 28 if keyboard_capture else 36 if battery_usb_write else 34 if battery_usb_read else 33 if battery_service else 32 if battery_transitions else 31 if battery_capture else 26 if power_telemetry else 25 if desktop_idle_capture else 24 if aml_capture else 23 if idle_capture else 19}-laptop-probe-{laptop_probe_volume}"
        if ap_coordinated_c3: version=f"ram-usb-v1-ap-c3-laptop-probe-{laptop_probe_volume}"
        if ap_package_audit: version=f"ram-usb-v1-ap-package-laptop-probe-{laptop_probe_volume}"
        if ap_ownership_test: version=f"ram-usb-v1-ap-owner-laptop-probe-{laptop_probe_volume}"
        if posix_capture: version=f"ram-usb-v1-posix-laptop-probe-{laptop_probe_volume}"
        if storage_wait_capture: version=f"ram-usb-v1-storage-wait-laptop-probe-{laptop_probe_volume}"
        if mwait_wakeup_test: version=f"ram-usb-v1-mwait-laptop-probe-{laptop_probe_volume}"
        if mwait_deep_test: version=f"ram-usb-v1-mwait-c3-laptop-probe-{laptop_probe_volume}"
        if not hold_on_failure and (not battery_transitions or battery_air61) and not keyboard_capture and not cpu_load_capture: version += "-auto-return"
        if diagnostic or autotest_volume is not None or run_id is not None or workspace_volume is not None or trackpad_run is not None or pointer or repeat != 1 or desktop_volume is not None or not LAPTOP_PROBE_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting automatic laptop probe profile")
    sources = {
        "loader/BOOTX64.EFI": ROOT / "build/limine-binary/BOOTX64.EFI",
        "srvros.elf": ROOT / ("build/ap-probe/srvros.elf" if (ap_ownership_test or ap_coordinated_c3) else "build/acpi-air61-battery/srvros.elf" if battery_air61 else "build/acpi-battery/srvros.elf" if (desktop_battery or battery_capture or battery_transitions or battery_service or battery_usb_read or battery_usb_write or battery_filesystem or battery_desktop or battery_text or battery_load or battery_app_files or battery_controls or battery_query or battery_smbus or battery_smbus_files or battery_home or battery_home_session or battery_home_desktop or battery_home_transitions or battery_home_work) else "build/kernel/srvros.elf"),
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
    elif verify(source).get("config_version", "").startswith(("ram-usb-v27-","ram-usb-v28-")):
        print("Guided keyboard test: on-screen prompts, up to 90 seconds per step, save result and return; halt if saving fails.")
    elif LAPTOP_PROBE_RE.fullmatch(verify(source).get("config_version", "")):
        print("Laptop diagnostics: new unattended stages return on handled failure; guided/held profiles retain the failure screen.")
    elif verify(source).get("config_version", "").startswith(("ram-usb-v77-","ram-usb-v81-")):
        print("Attended audio/USB load: Help checklist opens automatically; 30-minute load, then save and exit manually. No automatic reboot.")
    elif verify(source).get("config_version", "").startswith("ram-usb-v76-"):
        print("Attended window/dock/navigation checklist opens automatically in Help; no timer. Save, Return to console, then reboot manually.")
    elif verify(source).get("config_version", "").startswith("ram-usb-v75-"):
        print("Attended application lifecycle checklist opens automatically in Help; no timed exit. Save, Return to console, then reboot manually.")
    elif verify(source).get("config_version", "").startswith(("ram-usb-v74-","ram-usb-v80-")):
        print("Automatic audio/USB stress: 30 minutes of continuous playback and verified filesystem work; listen only, normal retirement/unmount and Ubuntu return.")
    elif verify(source).get("config_version", "").startswith("ram-usb-v73-"):
        print("Automatic capacity test: two rounds of 24 mixed apps, screenshots and normal retirement; returns after save/unmount, no input required.")
    elif verify(source).get("config_version", "").startswith(("ram-usb-v72-","ram-usb-v79-")):
        print("Automated two-session startup diagnostic; normal return to Ubuntu, fatal mounted ownership halts for photograph/recovery.")
    elif verify(source).get("config_version", "").startswith(("ram-usb-v54-","ram-usb-v55-","ram-usb-v70-","ram-usb-v71-")):
        print("Ordinary Home with opt-in battery monitoring; no timed exit. Ctrl+Alt+Esc returns to monitor; reboot flushes/unmounts.")
    elif verify(source).get("config_version", "").startswith(("ram-usb-v78-","ram-usb-v82-")):
        print("Automatic desktop power test: idle/app/recovery phases, saved diagnostics and normal unmount/Ubuntu return; no input required.")
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
    parser.add_argument("--desktop-navigation-trial",action="store_true",help="attended window/dock/keyboard checklist; opens Help automatically, no timed exit")
    parser.add_argument("--desktop-lifecycle-trial",action="store_true",help="attended owned-dialog/quit/reopen checklist; opens Help automatically, no timed exit")
    parser.add_argument("--desktop-capacity-test",action="store_true",help="automatic two-round 24-app capacity test; saves and returns without input")
    parser.add_argument("--desktop-audio-io-attended",action="store_true",help="attended Air desktop with automatic 30-minute audio/disk load and guest checklist; manual exit")
    parser.add_argument("--desktop-audio-io-test",action="store_true",help="automatic 30-minute Air PCM and verified USB stress; listening only")
    parser.add_argument("--desktop-reopen-test",action="store_true",help="automated two-session Air startup diagnostic; fatal mounted faults halt")
    parser.add_argument("--desktop-basic",action="store_true",help="unscaled desktop/input; optional --desktop-brightness, --desktop-audio and --desktop-sensors")
    parser.add_argument("--desktop-network",action="store_true",help="enable ECM on the scaled sensor desktop")
    parser.add_argument("--desktop-sensors",action="store_true",help="enable read-only CPU and fan telemetry in the explicit audio desktop")
    parser.add_argument("--desktop-audio",action="store_true",help="enable HDA in the explicit brightness trial")
    parser.add_argument("--desktop-brightness",action="store_true",help="enable brightness only in the scaled desktop trial")
    parser.add_argument("--desktop-device-checks", action="store_true", help="attended audio/Ethernet/input checklist with Help at startup; battery off")
    parser.add_argument("--desktop-scale",type=int,choices=(1,2),default=1,help="explicit desktop integer scale")
    parser.add_argument("--desktop-volume", type=compact_volume_uuid, help="stage an interactive native desktop trial for this compact USB UUID")
    parser.add_argument("--laptop-probe-volume", type=compact_volume_uuid, help="stage unattended laptop diagnostics on this prepared USB UUID")
    parser.add_argument("--desktop-power-test",action="store_true",help="automatic Air battery desktop idle/app/recovery power capture")
    parser.add_argument("--power-telemetry",action="store_true",help="opt-in read-only CPU energy/frequency diagnostics during --desktop-idle-capture; completion is not power acceptance")
    parser.add_argument("--hold-on-failure",action="store_true",help="explicit attended diagnostic: retain failure screen instead of automatic return")
    parser.add_argument("--posix-capture",action="store_true",help="automatic bounded POSIX, TLS, SQLite and desktop regression; saves and returns")
    capture=parser.add_mutually_exclusive_group()
    parser.add_argument("--ap-coordinated-c3",action="store_true",help="explicit coordinated C3 diagnostic; requires --cpu-load-capture")
    parser.add_argument("--ap-package-audit",action="store_true",help="Read-only package/PCI diagnostics during coordinated C3")
    parser.add_argument("--ap-ownership-test",action="store_true",help="separate MP diagnostic kernel with bounded secondary-CPU mailboxes")
    parser.add_argument("--mwait-deep-test",action="store_true",help="explicit bounded Haswell C3 hint trial; requires --cpu-load-capture")
    parser.add_argument("--mwait-wakeup-test",action="store_true",help="explicit native shallow wakeup trial; requires --cpu-load-capture")
    capture.add_argument("--cpu-load-capture",action="store_true",help="bounded idle/load/recovery diagnostic; save and return on handled failure")
    capture.add_argument("--keyboard-capture",action="store_true",help="guided typing test, save results and return automatically")
    capture.add_argument("--desktop-idle-capture",action="store_true",help="measure the desktop for 60 seconds, save screenshot/logs, unmount and return")
    capture.add_argument("--battery-home-transitions",action="store_true",help="guided Home AC unplug/reconnect capture; input required, automatic return")
    capture.add_argument("--battery-home-work",action="store_true",help="attended five-minute Home typing/file capture; save and exit normally")
    capture.add_argument("--battery-home-desktop",action="store_true",help="automatic 180-second ordinary Home desktop and battery capture")
    capture.add_argument("--battery-home-session",action="store_true",help="automatic held Home session with Text/file work; requires a fresh battery-study directory")
    capture.add_argument("--battery-home",action="store_true",help="automatic held mounted-Home battery boundary; no interactive input")
    capture.add_argument("--battery-smbus-files",action="store_true",help="automatic parked SMBus and Text file-worker capture; requires fresh image")
    capture.add_argument("--battery-smbus",action="store_true",help="automatic parked native SMBus host event capture")
    capture.add_argument("--battery-query",action="store_true",help="automatic parked query-refusal/retirement capture; returns if not exercised")
    capture.add_argument("--battery-controls",action="store_true",help="automatic System start/stop study with parked workspace")
    capture.add_argument("--battery-app-files",action="store_true",help="automatic Text file-worker and battery overlap; requires a fresh image")
    capture.add_argument("--battery-load",action="store_true",help="automatic RAM Text, isolated filesystem and battery overlap; requires a fresh image")
    capture.add_argument("--battery-text",action="store_true",help="automatic RAM-only full Text editing and battery overlap; leave input alone")
    capture.add_argument("--battery-desktop",action="store_true",help="automatic RAM-only compositor and cached battery overlap; no storage load")
    capture.add_argument("--battery-filesystem",action="store_true",help="automatic battery sampling with a prepared isolated filesystem image")
    capture.add_argument("--battery-usb-write",action="store_true",help="automatic battery sampling with bounded scratch data writes and flushes")
    capture.add_argument("--battery-usb-read",action="store_true",help="automatic battery sampling with write-revoked real USB reads")
    capture.add_argument("--battery-service",action="store_true",help="unattended cached-reader and service start/stop diagnostic; checkpoint and return even on failure")
    capture.add_argument("--battery-transitions",action="store_true",help="guided AC unplug/replug diagnostic; persistent instructions and bounded waits")
    capture.add_argument("--battery-capture",action="store_true",help="explicit native battery diagnostic kernel; save and return only after clean teardown")
    capture.add_argument("--aml-capture",action="store_true",help="run the isolated snapshot-only AML diagnostic with workspace logging; return only after clean completion")
    capture.add_argument("--idle-capture", action="store_true", help="sample idle scheduling and cached devices for 60 seconds, save, unmount and return")
    parser.add_argument("--desktop-battery",action="store_true",help="ordinary Home session with explicit battery monitoring; Ctrl+Alt+Esc exits")
    parser.add_argument("--desktop-services",action="store_true",help="add existing brightness, HDA, read-only sensors and ECM discovery to --desktop-battery")
    parser.add_argument("--battery-preflight",action="store_true",help="passive Air6,1 PM1/GPE/EC-status capture; no live battery sampling")
    parser.add_argument("--battery-air61",action="store_true",help="exact Air6,1 battery kernel; use a capture or --desktop-battery --desktop-volume")
    parser.add_argument("--storage-wait-capture",action="store_true",help="counterbalanced fixed-file USB polling/wait comparison; saves and returns automatically")
    args = parser.parse_args()
    if args.desktop_navigation_trial and (args.action != "stage" or not args.desktop_volume or not args.desktop_services or not args.desktop_battery):
        parser.error("--desktop-navigation-trial requires stage and combined battery/services desktop volume")
    if args.desktop_lifecycle_trial and (args.action != "stage" or not args.desktop_volume or not args.desktop_services or not args.desktop_battery):
        parser.error("--desktop-lifecycle-trial requires stage and combined battery/services desktop volume")
    if args.desktop_capacity_test and (args.action != "stage" or not args.desktop_volume or not args.desktop_services or not args.desktop_battery):
        parser.error("--desktop-capacity-test requires stage and combined battery/services desktop volume")
    if args.posix_capture and (args.action != "stage" or not args.laptop_probe_volume):
        parser.error("--posix-capture requires stage --laptop-probe-volume")
    if args.storage_wait_capture and (args.action != "stage" or not args.laptop_probe_volume):
        parser.error("--storage-wait-capture requires stage --laptop-probe-volume")
    if args.battery_air61 and (args.action != "stage" or not (args.laptop_probe_volume or (args.desktop_battery and args.desktop_volume))):
        parser.error("--battery-air61 requires stage with a laptop probe or explicit battery desktop volume")
    if args.battery_preflight and (args.action != "stage" or not args.laptop_probe_volume):
        parser.error("--battery-preflight requires stage --laptop-probe-volume")
    if args.desktop_services and (args.action != "stage" or not args.desktop_battery):
        parser.error("--desktop-services requires stage --desktop-battery --desktop-volume")
    if args.desktop_battery and (args.action != "stage" or not args.desktop_volume):
        parser.error("--desktop-battery requires stage --desktop-volume")
    if args.hold_on_failure and (args.action != "stage" or not args.laptop_probe_volume):
        parser.error("--hold-on-failure requires stage --laptop-probe-volume")
    if args.ap_package_audit and (args.action!="stage" or not args.ap_coordinated_c3):
        parser.error("Package audit requires explicit coordinated C3 staging")
    if args.ap_coordinated_c3 and (args.action!="stage" or not args.laptop_probe_volume or not args.cpu_load_capture):
        parser.error("--ap-coordinated-c3 requires stage --laptop-probe-volume --cpu-load-capture")
    if args.ap_ownership_test and (args.action!="stage" or not args.laptop_probe_volume):
        parser.error("--ap-ownership-test requires stage --laptop-probe-volume")
    if (args.mwait_wakeup_test or args.mwait_deep_test) and (args.action!="stage" or not args.cpu_load_capture):
        parser.error("--mwait-wakeup-test requires stage --cpu-load-capture")
    if (args.idle_capture or args.aml_capture or args.desktop_idle_capture or args.battery_capture or args.battery_transitions or args.battery_service or args.battery_usb_read or args.battery_usb_write or args.battery_filesystem or args.battery_desktop or args.battery_text or args.battery_load or args.battery_app_files or args.battery_controls or args.battery_query or args.battery_smbus or args.battery_smbus_files or args.battery_home or args.battery_home_session or args.battery_home_desktop or args.battery_home_transitions or args.battery_home_work or args.keyboard_capture or args.cpu_load_capture) and (args.action != "stage" or not args.laptop_probe_volume):
        parser.error("capture requires stage --laptop-probe-volume")
    if args.power_telemetry and (args.action != "stage" or not args.desktop_idle_capture):
        parser.error("--power-telemetry requires stage --desktop-idle-capture")
    if args.laptop_probe_volume and args.action != "stage":
        parser.error("--laptop-probe-volume applies only to stage")
    if args.keep_previous and args.action != "install":
        parser.error("--keep-previous applies only to install")
    if args.release and args.action != "install":
        parser.error("--release applies only to install")
    if args.desktop_device_checks and args.action != "stage":
        parser.error("--desktop-device-checks applies only to stage")
    if args.desktop_network and args.action != "stage":
        parser.error("--desktop-network applies only to stage")
    if args.desktop_sensors and args.action != "stage":
        parser.error("--desktop-sensors applies only to stage")
    if args.desktop_audio and args.action != "stage":
        parser.error("--desktop-audio applies only to stage")
    if args.desktop_brightness and args.action != "stage":
        parser.error("--desktop-brightness applies only to stage")
    if args.desktop_volume and args.action != "stage":
        parser.error("--desktop-volume applies only to stage")
    if args.diagnostic and args.action != "stage":
        parser.error("--diagnostic applies only to stage")
    try:
        if args.action == "stage":
            stage(args.diagnostic, desktop_volume=args.desktop_volume, laptop_probe_volume=args.laptop_probe_volume, idle_capture=args.idle_capture, aml_capture=args.aml_capture, desktop_idle_capture=args.desktop_idle_capture, power_telemetry=args.power_telemetry, battery_capture=args.battery_capture, battery_transitions=args.battery_transitions, battery_service=args.battery_service, hold_on_failure=args.hold_on_failure, battery_usb_read=args.battery_usb_read, battery_usb_write=args.battery_usb_write, battery_filesystem=args.battery_filesystem, battery_desktop=args.battery_desktop, battery_text=args.battery_text, battery_load=args.battery_load, battery_app_files=args.battery_app_files, battery_controls=args.battery_controls, battery_query=args.battery_query, battery_smbus=args.battery_smbus, battery_smbus_files=args.battery_smbus_files, battery_home=args.battery_home,battery_home_session=args.battery_home_session,battery_home_desktop=args.battery_home_desktop,battery_home_transitions=args.battery_home_transitions,battery_home_work=args.battery_home_work, keyboard_capture=args.keyboard_capture, cpu_load_capture=args.cpu_load_capture, desktop_scale=args.desktop_scale, desktop_brightness=args.desktop_brightness, desktop_audio=args.desktop_audio, desktop_sensors=args.desktop_sensors, desktop_network=args.desktop_network, desktop_basic=args.desktop_basic, desktop_battery=args.desktop_battery, desktop_services=args.desktop_services, desktop_device_checks=args.desktop_device_checks,battery_preflight=args.battery_preflight,battery_air61=args.battery_air61,storage_wait_capture=args.storage_wait_capture,desktop_reopen_test=args.desktop_reopen_test,posix_capture=args.posix_capture,desktop_capacity_test=args.desktop_capacity_test,desktop_lifecycle_trial=args.desktop_lifecycle_trial,desktop_audio_io_test=args.desktop_audio_io_test,desktop_audio_io_attended=args.desktop_audio_io_attended,desktop_navigation_trial=args.desktop_navigation_trial,desktop_power_test=args.desktop_power_test,mwait_wakeup_test=args.mwait_wakeup_test,mwait_deep_test=args.mwait_deep_test,ap_ownership_test=args.ap_ownership_test,ap_coordinated_c3=args.ap_coordinated_c3,ap_package_audit=args.ap_package_audit)
        else:
            install(args.release, keep_previous=args.keep_previous)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"hardware-boot: {error}\n")


if __name__ == "__main__":
    main()
