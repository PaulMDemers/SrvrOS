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
LAPTOP_PROBE_RE = re.compile(r"ram-usb-(?:wifi-fw-v(?:[1-9]|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41)|wifi-cal-v1|v(?:19|23|24|25|26|27|28|31|32|33|34|35|36|38|39|40|41|42|43|44|45|46|47|48|49))-laptop-probe-([0-9a-f]{8})(-auto-return)?")
DESKTOP_RE = re.compile(r"ram-usb-v(?:17|18|20|21|22|29|30|31|36|37)-desktop-([0-9a-f]{8})")
TRACKPAD_RE = re.compile(r"ram-usb-v(?:10|11|12|13|14|15|16)-trackpad-([0-9a-f]{8})-([0-9a-f]{16})")


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def release_id(hashes, version=CONFIG_VERSION):
    # Configuration version is included so a command-line change creates a new release.
    return hashlib.sha256((version + "\n" + json.dumps(hashes, sort_keys=True)).encode()).hexdigest()[:16]


def limine_config(release, version=CONFIG_VERSION, *, legacy_wifi_calibration=False):
    if legacy_wifi_calibration and not version.startswith(("ram-usb-v43-","ram-usb-v44-")):
        raise ValueError("Legacy calibration applies only to collided v43/v44")
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
        if version.startswith(("ram-usb-v31-","ram-usb-v32-","ram-usb-v33-","ram-usb-v34-","ram-usb-v36-","ram-usb-v38-","ram-usb-v40-","ram-usb-v41-","ram-usb-v42-","ram-usb-v43-","ram-usb-v44-","ram-usb-v45-","ram-usb-v46-","ram-usb-v47-","ram-usb-v48-","ram-usb-v49-")):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=battery")
            if version.startswith("ram-usb-v32-"): cmdline += "-transitions"
            if version.startswith("ram-usb-v33-"): cmdline += "-service"
            if version.startswith("ram-usb-v34-"): cmdline += "-usb-read"
            if version.startswith("ram-usb-v36-"): cmdline += "-usb-write"
            if version.startswith("ram-usb-v38-"): cmdline += "-filesystem"
            if version.startswith("ram-usb-v40-"): cmdline += "-desktop srvros.trackpad=gestures"
            if version.startswith("ram-usb-v41-"): cmdline += "-text srvros.trackpad=gestures"
            if version.startswith("ram-usb-v49-"): cmdline += "-home-session srvros.trackpad=gestures"
            if version.startswith("ram-usb-v48-"): cmdline += "-home"
            if version.startswith("ram-usb-v47-"): cmdline += "-smbus-files srvros.trackpad=gestures"
            if version.startswith("ram-usb-v46-"): cmdline += "-smbus"
            if version.startswith("ram-usb-v45-"): cmdline += "-query"
            if version.startswith("ram-usb-v44-"): cmdline += "-controls srvros.trackpad=gestures"
            if version.startswith("ram-usb-v43-"): cmdline += "-app-files srvros.trackpad=gestures"
            if version.startswith("ram-usb-v42-"): cmdline += "-load srvros.trackpad=gestures"
        if version.startswith("ram-usb-v35-"):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=cpu-load "
                       "srvros.sensors=cpu srvros.smc=read-only srvros.power=read-only")
        if version.startswith("ram-usb-v39-"):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=wifi srvros.usb-wifi=probe")
        if version.startswith("ram-usb-wifi-cal-v1-") or legacy_wifi_calibration:
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=wifi-calibration srvros.usb-wifi=probe")
        if version.startswith(("ram-usb-wifi-fw-v1-","ram-usb-wifi-fw-v2-","ram-usb-wifi-fw-v3-","ram-usb-wifi-fw-v4-","ram-usb-wifi-fw-v5-","ram-usb-wifi-fw-v6-","ram-usb-wifi-fw-v7-","ram-usb-wifi-fw-v8-","ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")):
            cmdline = (f"srvros.storage=ram srvros.workspace-volume={probe[1]} "
                       f"srvros.desktop-release={release} srvros.capture=wifi-firmware srvros.usb-wifi=probe")
        if version.startswith(("ram-usb-wifi-fw-v2-","ram-usb-wifi-fw-v3-","ram-usb-wifi-fw-v4-","ram-usb-wifi-fw-v5-","ram-usb-wifi-fw-v6-","ram-usb-wifi-fw-v7-","ram-usb-wifi-fw-v8-","ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mac-startup=1"
        if version.startswith(("ram-usb-wifi-fw-v3-","ram-usb-wifi-fw-v4-","ram-usb-wifi-fw-v5-","ram-usb-wifi-fw-v6-","ram-usb-wifi-fw-v7-","ram-usb-wifi-fw-v8-","ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mcu-probe=1"
        if version.startswith(("ram-usb-wifi-fw-v4-","ram-usb-wifi-fw-v5-","ram-usb-wifi-fw-v6-","ram-usb-wifi-fw-v7-","ram-usb-wifi-fw-v8-","ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-radio-reset=1"
        if version.startswith(("ram-usb-wifi-fw-v5-","ram-usb-wifi-fw-v6-","ram-usb-wifi-fw-v7-","ram-usb-wifi-fw-v8-","ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-write-probe=1"
        if version.startswith(("ram-usb-wifi-fw-v6-","ram-usb-wifi-fw-v7-","ram-usb-wifi-fw-v8-","ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-pair-probe=1"
        if version.startswith(("ram-usb-wifi-fw-v7-","ram-usb-wifi-fw-v8-","ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-timing-probe=1"
        if version.startswith(("ram-usb-wifi-fw-v8-","ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mac-basics=1"
        if version.startswith(("ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mac-format=1"
        if version.startswith(("ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-bbp-ready=1"
        if version.startswith(("ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-bbp-core=1"
        if version.startswith(("ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-bbp-front=1"
        if version.startswith(("ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-bbp-agc=1"
        if version.startswith(("ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-bbp-batch=1"
        if version.startswith(("ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-compact=1"
        if version.startswith(("ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-agc-more=1"
        if version.startswith(("ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-agc-next=1"
        if version.startswith(("ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-agc-tail=1"
        if version.startswith(("ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-rxc-start=1"
        if version.startswith(("ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-rxc-tail=1"
        if version.startswith(("ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-txbe-start=1"
        if version.startswith("ram-usb-wifi-fw-v22-"): cmdline += " srvros.wifi-txbe-next=1"
        if version.startswith("ram-usb-wifi-fw-v23-"): cmdline += " srvros.wifi-txbe-observe=1"
        if version.startswith("ram-usb-wifi-fw-v24-"): cmdline += " srvros.wifi-txbe-first=1"
        if version.startswith("ram-usb-wifi-fw-v25-"): cmdline += " srvros.wifi-txbe-second=1"
        if version.startswith("ram-usb-wifi-fw-v26-"): cmdline += " srvros.wifi-txbe-access=1"
        if version.startswith("ram-usb-wifi-fw-v27-"): cmdline += " srvros.wifi-txbe-direct=1"
        if version.startswith("ram-usb-wifi-fw-v28-"): cmdline += " srvros.wifi-clock=1"
        if version.startswith("ram-usb-wifi-fw-v29-"): cmdline += " srvros.wifi-clock-direct=1"
        if version.startswith("ram-usb-wifi-fw-v30-"): cmdline += " srvros.wifi-clock-pair=1"
        if version.startswith(("ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-bbp-table=1"
        if version.startswith(("ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-")): cmdline += " srvros.wifi-mac-snapshot=1"
        if version.startswith("ram-usb-wifi-fw-v33-"): cmdline += " srvros.wifi-mac-raw=1"
        if version.startswith(("ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mac-route=1"
        if version.startswith(("ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mac-policy=1"
        if version.startswith("ram-usb-wifi-fw-v36-"): cmdline += " srvros.wifi-mac-policy-observe=1"
        if version.startswith(("ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mac-policy-validate=1"
        if version.startswith(("ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mac-protect=1"
        if version.startswith(("ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mac-contention=1"
        if version.startswith(("ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")): cmdline += " srvros.wifi-mac-txsw=1"
        if version.startswith("ram-usb-wifi-fw-v41-"): cmdline += " srvros.wifi-mac-beacon=1"
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
    expected = [limine_config(release, version)]
    # Independently published v43/v44 Wi-Fi and battery recipes collided.
    # Accept only either exact historical recipe, with identical config copies.
    # New calibration stages use wifi-cal-v1; never mint another ambiguous release.
    if version.startswith(("ram-usb-v43-","ram-usb-v44-")):
        expected.append(limine_config(release, version, legacy_wifi_calibration=True))
    config = (directory / "limine.conf").read_text()
    if manifest["release"] != release or config not in expected or (directory / "loader/limine.conf").read_text() != config:
        raise ValueError("Boot configuration or release identity mismatch")

    if version.startswith(("ram-usb-wifi-fw-v1-","ram-usb-wifi-fw-v2-","ram-usb-wifi-fw-v3-","ram-usb-wifi-fw-v4-","ram-usb-wifi-fw-v5-","ram-usb-wifi-fw-v6-","ram-usb-wifi-fw-v7-","ram-usb-wifi-fw-v8-","ram-usb-wifi-fw-v9-","ram-usb-wifi-fw-v10-","ram-usb-wifi-fw-v11-","ram-usb-wifi-fw-v12-","ram-usb-wifi-fw-v13-","ram-usb-wifi-fw-v14-","ram-usb-wifi-fw-v15-","ram-usb-wifi-fw-v16-","ram-usb-wifi-fw-v17-","ram-usb-wifi-fw-v18-","ram-usb-wifi-fw-v19-","ram-usb-wifi-fw-v20-","ram-usb-wifi-fw-v21-","ram-usb-wifi-fw-v22-","ram-usb-wifi-fw-v23-","ram-usb-wifi-fw-v24-","ram-usb-wifi-fw-v25-","ram-usb-wifi-fw-v26-","ram-usb-wifi-fw-v27-","ram-usb-wifi-fw-v28-","ram-usb-wifi-fw-v29-","ram-usb-wifi-fw-v30-","ram-usb-wifi-fw-v31-","ram-usb-wifi-fw-v32-","ram-usb-wifi-fw-v33-","ram-usb-wifi-fw-v34-","ram-usb-wifi-fw-v35-","ram-usb-wifi-fw-v36-","ram-usb-wifi-fw-v37-","ram-usb-wifi-fw-v38-","ram-usb-wifi-fw-v39-","ram-usb-wifi-fw-v40-","ram-usb-wifi-fw-v41-")):
        from wifi_firmware_package import verify_initramfs
        verify_initramfs(directory / "initramfs.tar")
    return manifest


def stage(diagnostic=False, autotest_volume=None, run_id=None, workspace_volume=None, repeat=1, trackpad_run=None, pointer=False, gestures=False, desktop_io=False, desktop_volume=None, laptop_probe_volume=None, idle_capture=False, aml_capture=False, desktop_idle_capture=False, power_telemetry=False, battery_capture=False, battery_transitions=False, battery_service=False, hold_on_failure=False, battery_usb_read=False, battery_usb_write=False, battery_filesystem=False, keyboard_capture=False, cpu_load_capture=False, desktop_scale=1, desktop_brightness=False, desktop_audio=False, desktop_sensors=False, desktop_network=False, battery_desktop=False, battery_text=False, battery_load=False, battery_app_files=False, battery_controls=False, battery_query=False, battery_smbus=False, battery_smbus_files=False, battery_home=False, battery_home_session=False, wifi_capture=False, wifi_calibration=False, wifi_firmware_package=None, wifi_mac_startup=False, wifi_mcu_probe=False, wifi_radio_reset=False, wifi_write_probe=False, wifi_pair_probe=False, wifi_timing_probe=False, wifi_mac_basics=False, wifi_mac_format=False, wifi_bbp_ready=False, wifi_bbp_core=False, wifi_bbp_front=False, wifi_bbp_agc=False, wifi_bbp_batch=False, wifi_compact=False, wifi_agc_more=False, wifi_agc_next=False, wifi_agc_tail=False, wifi_rxc_start=False, wifi_rxc_tail=False, wifi_txbe_start=False, wifi_txbe_next=False, wifi_txbe_observe=False, wifi_txbe_first=False, wifi_txbe_second=False, wifi_txbe_access=False, wifi_txbe_direct=False, wifi_clock=False, wifi_clock_direct=False, wifi_clock_pair=False, wifi_bbp_table=False, wifi_mac_snapshot=False, wifi_mac_raw=False, wifi_mac_route=False, wifi_mac_policy=False, wifi_mac_policy_observe=False, wifi_mac_policy_validate=False, wifi_mac_protect=False, wifi_mac_contention=False, wifi_mac_txsw=False, wifi_mac_beacon=False):
    if (wifi_capture or wifi_calibration) and hold_on_failure:
        raise ValueError("Wi-Fi capture requires automatic return")
    if (battery_usb_read or battery_usb_write or battery_filesystem or battery_desktop or battery_text or battery_load or battery_app_files or battery_controls or battery_query or battery_smbus or battery_smbus_files or battery_home or battery_home_session) and hold_on_failure:
        raise ValueError("USB overlap requires automatic checkpoint/return")
    if hold_on_failure and laptop_probe_volume is None:
        raise ValueError("Hold-on-failure requires a laptop probe profile")
    if battery_transitions and battery_capture:
        raise ValueError("Battery sample and transition captures are separate profiles")
    if desktop_network and not desktop_sensors:
        raise ValueError("desktop network requires the scaled sensor/audio profile")
    if desktop_sensors and not desktop_audio:
        raise ValueError("desktop sensors require the scaled audio/brightness profile")
    if desktop_audio and not desktop_brightness:
        raise ValueError("desktop audio requires explicit scaled brightness profile")
    if desktop_brightness and (desktop_scale!=2 or desktop_volume is None):
        raise ValueError("desktop brightness requires a scaled desktop volume")
    if desktop_scale not in (1,2) or (desktop_scale!=1 and desktop_volume is None):
        raise ValueError("Desktop scale requires desktop volume and scale 1 or 2")
    if power_telemetry and not desktop_idle_capture:
        raise ValueError("Power telemetry requires the explicit desktop idle capture")
    if wifi_mac_beacon and not wifi_mac_txsw: raise ValueError("MAC beacon offsets require TX software control prerequisites")
    if wifi_mac_txsw and not wifi_mac_contention: raise ValueError("MAC TX software controls require contention prerequisites")
    if wifi_mac_contention and not wifi_mac_protect: raise ValueError("MAC contention requires protection prerequisites")
    if wifi_mac_protect and not wifi_mac_policy_validate: raise ValueError("MAC protection requires exact policy prerequisites")
    if wifi_mac_policy_validate and (not wifi_mac_policy or wifi_mac_policy_observe): raise ValueError("Policy validation requires policy without observation mode")
    if wifi_mac_policy_observe and not wifi_mac_policy: raise ValueError("Policy observation requires policy prerequisites")
    if wifi_mac_policy and not wifi_mac_route: raise ValueError("MAC policy requires route prerequisites")
    if wifi_mac_route and (not wifi_bbp_table or wifi_mac_snapshot or wifi_mac_raw): raise ValueError("MAC route requires BBP prerequisites without snapshot/raw modes")
    if wifi_mac_raw and not wifi_mac_snapshot: raise ValueError("Raw MAC inventory requires snapshot prerequisites")
    if wifi_mac_snapshot and not wifi_bbp_table: raise ValueError("MAC snapshot requires full BBP observation")
    if wifi_bbp_table and (not wifi_txbe_start or wifi_clock or wifi_clock_direct or wifi_clock_pair or wifi_txbe_next or wifi_txbe_observe or wifi_txbe_first or wifi_txbe_second or wifi_txbe_access or wifi_txbe_direct): raise ValueError("BBP table requires start and excludes other TXBE alternatives")
    if wifi_clock_pair and (not wifi_txbe_start or wifi_clock or wifi_clock_direct or wifi_txbe_next or wifi_txbe_observe or wifi_txbe_first or wifi_txbe_second or wifi_txbe_access or wifi_txbe_direct): raise ValueError("Clock pair requires start and excludes other TXBE alternatives")
    if wifi_clock_direct and (not wifi_txbe_start or wifi_clock or wifi_txbe_next or wifi_txbe_observe or wifi_txbe_first or wifi_txbe_second or wifi_txbe_access or wifi_txbe_direct): raise ValueError("Clock direct requires start and excludes other TXBE alternatives")
    if wifi_clock and (not wifi_txbe_start or wifi_txbe_next or wifi_txbe_observe or wifi_txbe_first or wifi_txbe_second or wifi_txbe_access or wifi_txbe_direct): raise ValueError("Clock diagnostic requires start and excludes next/observe/first/second/access/direct")
    if wifi_txbe_direct and (not wifi_txbe_start or wifi_txbe_next or wifi_txbe_observe or wifi_txbe_first or wifi_txbe_second or wifi_txbe_access): raise ValueError("TXBE direct requires start and excludes next/observe/first/second/access")
    if wifi_txbe_access and (not wifi_txbe_start or wifi_txbe_next or wifi_txbe_observe or wifi_txbe_first or wifi_txbe_second): raise ValueError("TXBE access requires start and excludes next/observe/first/second")
    if wifi_txbe_second and (not wifi_txbe_start or wifi_txbe_next or wifi_txbe_observe or wifi_txbe_first): raise ValueError("TXBE second requires start and excludes next/observe/first")
    if wifi_txbe_first and (not wifi_txbe_start or wifi_txbe_next or wifi_txbe_observe): raise ValueError("TXBE first requires start and excludes next/observe")
    if wifi_txbe_observe and (not wifi_txbe_start or wifi_txbe_next): raise ValueError("TXBE observation requires start and excludes next")
    if wifi_txbe_next and not wifi_txbe_start: raise ValueError("TXBE next requires TXBE start phase")
    if wifi_txbe_start and not wifi_rxc_tail: raise ValueError("TXBE start requires RXC tail phase")
    if wifi_rxc_tail and not wifi_rxc_start: raise ValueError("RXC tail requires RXC start phase")
    if wifi_rxc_start and not wifi_agc_tail: raise ValueError("RXC start requires AGC tail phase")
    if wifi_agc_tail and not wifi_agc_next: raise ValueError("AGC tail requires previous AGC phase")
    if wifi_agc_next and not wifi_agc_more: raise ValueError("Next AGC requires previous AGC phase")
    if wifi_agc_more and not wifi_compact: raise ValueError("Additional AGC requires compact setup")
    if wifi_compact and not wifi_bbp_batch: raise ValueError("Compact setup requires batching profile")
    if wifi_bbp_batch and not wifi_bbp_agc: raise ValueError("BBP batch requires AGC setup")
    if wifi_bbp_agc and not wifi_bbp_front: raise ValueError("BBP AGC requires initial IBI/AGC setup")
    if wifi_bbp_front and not wifi_bbp_core: raise ValueError("BBP front requires CORE setup")
    if wifi_bbp_core and not wifi_bbp_ready: raise ValueError("BBP CORE requires readiness")
    if wifi_bbp_ready and not wifi_mac_format: raise ValueError("BBP readiness requires MAC format")
    if wifi_mac_format and not wifi_mac_basics: raise ValueError("MAC format requires MAC basics")
    if wifi_mac_basics and not wifi_timing_probe: raise ValueError("MAC basics requires timing probe")
    if wifi_timing_probe and not wifi_pair_probe: raise ValueError("Timing probe requires pair probe")
    if wifi_pair_probe and not wifi_write_probe: raise ValueError("Pair probe requires write probe")
    if wifi_write_probe and not wifi_radio_reset: raise ValueError("Write probe requires radio reset")
    if wifi_radio_reset and not wifi_mcu_probe: raise ValueError("Radio reset requires MCU probe")
    if wifi_mcu_probe and (not wifi_firmware_package or not wifi_mac_startup): raise ValueError("MCU probe requires firmware package and MAC startup")
    if wifi_mac_startup and not wifi_firmware_package: raise ValueError("MAC startup requires firmware package")
    if (idle_capture or aml_capture or desktop_idle_capture or battery_capture or battery_transitions or battery_service or battery_usb_read or battery_usb_write or battery_filesystem or battery_desktop or battery_text or battery_load or battery_app_files or battery_controls or battery_query or battery_smbus or battery_smbus_files or battery_home or battery_home_session or keyboard_capture or cpu_load_capture or wifi_capture or wifi_calibration or wifi_firmware_package) and laptop_probe_volume is None:
        raise ValueError("Idle/AML capture requires a laptop probe volume")
    if sum(map(bool,(idle_capture,aml_capture,desktop_idle_capture,battery_capture,battery_transitions,battery_service,battery_usb_read,battery_usb_write,battery_filesystem,battery_desktop,battery_text,battery_load,battery_app_files,battery_controls,battery_query,battery_smbus,battery_smbus_files,battery_home,battery_home_session,keyboard_capture,cpu_load_capture,wifi_capture,wifi_calibration,bool(wifi_firmware_package))))>1:
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
        version = f"ram-usb-v{37 if desktop_network else 36 if desktop_sensors else 31 if desktop_audio else 30 if desktop_brightness else 29 if desktop_scale==2 else 22}-desktop-{desktop_volume}"
        if diagnostic or autotest_volume is not None or run_id is not None or workspace_volume is not None or trackpad_run is not None or pointer or repeat != 1 or not DESKTOP_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting native desktop profile")
    if laptop_probe_volume is not None:
        version=f"ram-usb-v{39 if wifi_capture else 49 if battery_home_session else 48 if battery_home else 47 if battery_smbus_files else 46 if battery_smbus else 45 if battery_query else 44 if battery_controls else 43 if battery_app_files else 42 if battery_load else 41 if battery_text else 40 if battery_desktop else 38 if battery_filesystem else 35 if cpu_load_capture else 28 if keyboard_capture else 36 if battery_usb_write else 34 if battery_usb_read else 33 if battery_service else 32 if battery_transitions else 31 if battery_capture else 26 if power_telemetry else 25 if desktop_idle_capture else 24 if aml_capture else 23 if idle_capture else 19}-laptop-probe-{laptop_probe_volume}"
        if wifi_calibration: version=f"ram-usb-wifi-cal-v1-laptop-probe-{laptop_probe_volume}"
        if wifi_firmware_package: version=f"ram-usb-wifi-fw-v{41 if wifi_mac_beacon else 40 if wifi_mac_txsw else 39 if wifi_mac_contention else 38 if wifi_mac_protect else 37 if wifi_mac_policy_validate else 36 if wifi_mac_policy_observe else 35 if wifi_mac_policy else 34 if wifi_mac_route else 33 if wifi_mac_raw else 32 if wifi_mac_snapshot else 31 if wifi_bbp_table else 30 if wifi_clock_pair else 29 if wifi_clock_direct else 28 if wifi_clock else 27 if wifi_txbe_direct else 26 if wifi_txbe_access else 25 if wifi_txbe_second else 24 if wifi_txbe_first else 23 if wifi_txbe_observe else 22 if wifi_txbe_next else 21 if wifi_txbe_start else 20 if wifi_rxc_tail else 19 if wifi_rxc_start else 18 if wifi_agc_tail else 17 if wifi_agc_next else 16 if wifi_agc_more else 15 if wifi_compact else 14 if wifi_bbp_batch else 13 if wifi_bbp_agc else 12 if wifi_bbp_front else 11 if wifi_bbp_core else 10 if wifi_bbp_ready else 9 if wifi_mac_format else 8 if wifi_mac_basics else 7 if wifi_timing_probe else 6 if wifi_pair_probe else 5 if wifi_write_probe else 4 if wifi_radio_reset else 3 if wifi_mcu_probe else 2 if wifi_mac_startup else 1}-laptop-probe-{laptop_probe_volume}"
        if not hold_on_failure and not battery_transitions and not keyboard_capture and not cpu_load_capture: version += "-auto-return"
        if diagnostic or autotest_volume is not None or run_id is not None or workspace_volume is not None or trackpad_run is not None or pointer or repeat != 1 or desktop_volume is not None or not LAPTOP_PROBE_RE.fullmatch(version):
            raise ValueError("Invalid or conflicting automatic laptop probe profile")
    sources = {
        "loader/BOOTX64.EFI": ROOT / "build/limine-binary/BOOTX64.EFI",
        "srvros.elf": ROOT / ("build/acpi-battery/srvros.elf" if (battery_capture or battery_transitions or battery_service or battery_usb_read or battery_usb_write or battery_filesystem or battery_desktop or battery_text or battery_load or battery_app_files or battery_controls or battery_query or battery_smbus or battery_smbus_files or battery_home or battery_home_session) else "build/kernel/srvros.elf"),
        "initramfs.tar": ROOT / "build/initramfs.tar",
    }
    STAGE.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="stage-", dir=STAGE) as tmp:
        target = Path(tmp)
        (target / "loader").mkdir()
        for name, source in sources.items():
            shutil.copyfile(source, target / name)
        if wifi_firmware_package:
            from wifi_firmware_package import add_to_initramfs
            add_to_initramfs(Path(wifi_firmware_package), target / "initramfs.tar")
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
    parser.add_argument("--desktop-network",action="store_true",help="enable ECM on the scaled sensor desktop")
    parser.add_argument("--desktop-sensors",action="store_true",help="enable read-only CPU and fan telemetry in the scaled audio desktop")
    parser.add_argument("--desktop-audio",action="store_true",help="enable HDA in the scaled brightness trial")
    parser.add_argument("--desktop-brightness",action="store_true",help="enable brightness only in the scaled desktop trial")
    parser.add_argument("--desktop-scale",type=int,choices=(1,2),default=1,help="explicit desktop integer scale")
    parser.add_argument("--desktop-volume", type=compact_volume_uuid, help="stage an interactive native desktop trial for this compact USB UUID")
    parser.add_argument("--laptop-probe-volume", type=compact_volume_uuid, help="stage unattended laptop diagnostics on this prepared USB UUID")
    parser.add_argument("--power-telemetry",action="store_true",help="opt-in read-only CPU energy/frequency diagnostics during --desktop-idle-capture; completion is not power acceptance")
    parser.add_argument("--hold-on-failure",action="store_true",help="explicit attended diagnostic: retain failure screen instead of automatic return")
    capture=parser.add_mutually_exclusive_group()
    capture.add_argument("--wifi-calibration",action="store_true",help="capture logical EFUSE after logging, redact MAC, save and return")
    parser.add_argument("--wifi-mac-route",action="store_true",help="fixed FCE/WMM initialization; no BBP table writes")
    parser.add_argument("--wifi-mac-policy", action="store_true", help="Bounded common MAC policy trial")
    parser.add_argument("--wifi-mac-policy-observe", action="store_true", help="Observe all policy readbacks without accepting mismatches")
    parser.add_argument("--wifi-mac-policy-validate", action="store_true", help="Validate captured exact policy readbacks")
    parser.add_argument("--wifi-mac-protect", action="store_true", help="Bounded CCK OFDM GF protection trial")
    parser.add_argument("--wifi-mac-contention", action="store_true", help="Bounded mixed mode TXOP RTS trial")
    parser.add_argument("--wifi-mac-txsw", action="store_true", help="Bounded TX software control trial")
    parser.add_argument("--wifi-mac-beacon", action="store_true", help="Bounded beacon-offset initialization trial")
    parser.add_argument("--wifi-mac-raw",action="store_true",help="raw read-only MAC inventory with transport receipts; no BBP writes")
    parser.add_argument("--wifi-mac-snapshot",action="store_true",help="read-only MAC prerequisite inventory before BBP observation")
    parser.add_argument("--wifi-bbp-table",action="store_true",help="full baseband table observation, not register acceptance")
    parser.add_argument("--wifi-clock-pair",action="store_true",help="clock prerequisite and ordered TXBE9/TXBE10 pair")
    parser.add_argument("--wifi-clock-direct",action="store_true",help="clock prerequisite plus isolated TXBE10 zero write")
    parser.add_argument("--wifi-clock",action="store_true",help="bounded PBF/MAC clock prerequisite and TXBE10 read comparison")
    parser.add_argument("--wifi-txbe-direct",action="store_true",help="isolated EP0 TXBE10 zero write and MCU/EP0 readback")
    parser.add_argument("--wifi-txbe-access",action="store_true",help="read-only MCU/EP0 comparison; requires start, excludes next/observe/first/second")
    parser.add_argument("--wifi-txbe-second",action="store_true",help="isolated TXBE10 comparison; requires start, excludes next/observe/first")
    parser.add_argument("--wifi-txbe-first",action="store_true",help="isolated TXBE9 comparison; requires start, excludes next/observe")
    parser.add_argument("--wifi-txbe-observe",action="store_true",help="read-only TXBE samples; requires start, excludes next")
    parser.add_argument("--wifi-txbe-next",action="store_true",help="fixed TXBE9/10/12/13; requires --wifi-txbe-start")
    parser.add_argument("--wifi-txbe-start",action="store_true",help="fixed TXBE0/4/6/8; requires --wifi-rxc-tail")
    parser.add_argument("--wifi-rxc-tail",action="store_true",help="fixed RXC4/5/7 and TXO8; requires --wifi-rxc-start")
    parser.add_argument("--wifi-rxc-start",action="store_true",help="fixed TXC1 and RXC1/2/3; requires --wifi-agc-tail")
    parser.add_argument("--wifi-agc-tail",action="store_true",help="fixed AGC57/59/63 and TXC0; requires --wifi-agc-next")
    parser.add_argument("--wifi-agc-next",action="store_true",help="fixed AGC33/34/37/41; requires --wifi-agc-more")
    parser.add_argument("--wifi-agc-more",action="store_true",help="fixed AGC23/24/25/26; requires --wifi-compact")
    parser.add_argument("--wifi-compact",action="store_true",help="five fixed four-pair phases; requires --wifi-bbp-batch")
    parser.add_argument("--wifi-bbp-batch",action="store_true",help="four-pair comparison; requires --wifi-bbp-agc")
    parser.add_argument("--wifi-bbp-agc",action="store_true",help="fixed AGC1/2/15/22 setup; requires --wifi-bbp-front")
    parser.add_argument("--wifi-bbp-front",action="store_true",help="fixed IBI/AGC and remaining CORE setup; requires --wifi-bbp-core")
    parser.add_argument("--wifi-bbp-core",action="store_true",help="fixed BBP CORE setup; requires --wifi-bbp-ready")
    parser.add_argument("--wifi-bbp-ready",action="store_true",help="read-only BBP readiness; requires --wifi-mac-format")
    parser.add_argument("--wifi-mac-format",action="store_true",help="fixed frame-format setup; requires --wifi-mac-basics")
    parser.add_argument("--wifi-mac-basics",action="store_true",help="fixed MAC basics initialization; requires --wifi-timing-probe")
    parser.add_argument("--wifi-timing-probe",action="store_true",help="capture distinct timing pairs with readback; requires --wifi-pair-probe")
    parser.add_argument("--wifi-pair-probe",action="store_true",help="capture two disabled-MAC write pairs; requires --wifi-write-probe")
    parser.add_argument("--wifi-write-probe",action="store_true",help="capture one disabled-MAC write reply; requires --wifi-radio-reset")
    parser.add_argument("--wifi-radio-reset",action="store_true",help="opt-in MAC/BBP reset before MCU probe; requires --wifi-mcu-probe")
    parser.add_argument("--wifi-mcu-probe",action="store_true",help="one-shot firmware register-read response capture; requires MAC startup and firmware package")
    parser.add_argument("--wifi-mac-startup",action="store_true",help="opt-in upstream MAC startup before MCU reset; requires firmware package")
    capture.add_argument("--wifi-firmware-package", type=Path, help="verified local mt7610e package; automatic MCU-readiness capture")
    capture.add_argument("--wifi-capture",action="store_true",help="read captured Wi-Fi identity, save diagnostics and return automatically")
    capture.add_argument("--cpu-load-capture",action="store_true",help="bounded idle/load/recovery diagnostic; save and return on handled failure")
    capture.add_argument("--keyboard-capture",action="store_true",help="guided typing test, save results and return automatically")
    capture.add_argument("--desktop-idle-capture",action="store_true",help="measure the desktop for 60 seconds, save screenshot/logs, unmount and return")
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
    args = parser.parse_args()
    if args.wifi_mac_beacon and (args.action != "stage" or not args.wifi_mac_txsw): parser.error("MAC beacon offsets require stage --wifi-mac-txsw")
    if args.wifi_mac_txsw and (args.action != "stage" or not args.wifi_mac_contention): parser.error("MAC TX software controls require stage --wifi-mac-contention")
    if args.wifi_mac_contention and (args.action != "stage" or not args.wifi_mac_protect): parser.error("MAC contention requires stage --wifi-mac-protect")
    if args.wifi_mac_protect and (args.action != "stage" or not args.wifi_mac_policy_validate): parser.error("MAC protection requires stage --wifi-mac-policy-validate")
    if args.wifi_mac_policy_validate and (args.action != "stage" or not args.wifi_mac_policy or args.wifi_mac_policy_observe): parser.error("Policy validation requires stage --wifi-mac-policy without observation mode")
    if args.wifi_mac_policy_observe and (args.action != "stage" or not args.wifi_mac_policy): parser.error("Policy observation requires stage --wifi-mac-policy")
    if args.wifi_mac_policy and (args.action != "stage" or not args.wifi_mac_route): parser.error("MAC policy requires stage --wifi-mac-route")
    if args.wifi_mac_route and (args.action != "stage" or not args.wifi_bbp_table or args.wifi_mac_snapshot or args.wifi_mac_raw): parser.error("MAC route requires stage --wifi-bbp-table without raw/snapshot modes")
    if args.wifi_mac_raw and (args.action != "stage" or not args.wifi_mac_snapshot): parser.error("Raw inventory requires stage --wifi-mac-snapshot")
    if args.wifi_mac_snapshot and (args.action != "stage" or not args.wifi_bbp_table): parser.error("MAC snapshot requires stage --wifi-bbp-table")
    if args.wifi_bbp_table and (args.action != "stage" or not args.wifi_txbe_start or args.wifi_clock or args.wifi_clock_direct or args.wifi_clock_pair or args.wifi_txbe_next or args.wifi_txbe_observe or args.wifi_txbe_first or args.wifi_txbe_second or args.wifi_txbe_access or args.wifi_txbe_direct):
        parser.error("--wifi-bbp-table requires stage --wifi-txbe-start and excludes other TXBE alternatives")
    if args.wifi_clock_pair and (args.action != "stage" or not args.wifi_txbe_start or args.wifi_clock or args.wifi_clock_direct or args.wifi_txbe_next or args.wifi_txbe_observe or args.wifi_txbe_first or args.wifi_txbe_second or args.wifi_txbe_access or args.wifi_txbe_direct):
        parser.error("--wifi-clock-pair requires stage --wifi-txbe-start and excludes other TXBE alternatives")
    if args.wifi_clock_direct and (args.action != "stage" or not args.wifi_txbe_start or args.wifi_clock or args.wifi_txbe_next or args.wifi_txbe_observe or args.wifi_txbe_first or args.wifi_txbe_second or args.wifi_txbe_access or args.wifi_txbe_direct):
        parser.error("--wifi-clock-direct requires stage --wifi-txbe-start and excludes other TXBE alternatives")
    if args.wifi_clock and (args.action != "stage" or not args.wifi_txbe_start or args.wifi_txbe_next or args.wifi_txbe_observe or args.wifi_txbe_first or args.wifi_txbe_second or args.wifi_txbe_access or args.wifi_txbe_direct):
        parser.error("--wifi-clock requires stage --wifi-txbe-start and excludes next/observe/first/second/access/direct")
    if args.wifi_txbe_direct and (args.action != "stage" or not args.wifi_txbe_start or args.wifi_txbe_next or args.wifi_txbe_observe or args.wifi_txbe_first or args.wifi_txbe_second or args.wifi_txbe_access):
        parser.error("--wifi-txbe-direct requires stage --wifi-txbe-start and excludes next/observe/first/second/access")
    if args.wifi_txbe_access and (args.action != "stage" or not args.wifi_txbe_start or args.wifi_txbe_next or args.wifi_txbe_observe or args.wifi_txbe_first or args.wifi_txbe_second):
        parser.error("--wifi-txbe-access requires stage --wifi-txbe-start and excludes next/observe/first/second")
    if args.wifi_txbe_second and (args.action != "stage" or not args.wifi_txbe_start or args.wifi_txbe_next or args.wifi_txbe_observe or args.wifi_txbe_first):
        parser.error("--wifi-txbe-second requires stage --wifi-txbe-start and excludes next/observe/first")
    if args.wifi_txbe_first and (args.action != "stage" or not args.wifi_txbe_start or args.wifi_txbe_next or args.wifi_txbe_observe):
        parser.error("--wifi-txbe-first requires stage --wifi-txbe-start and excludes next/observe")
    if args.wifi_txbe_observe and (args.action != "stage" or not args.wifi_txbe_start or args.wifi_txbe_next):
        parser.error("--wifi-txbe-observe requires stage --wifi-txbe-start and excludes --wifi-txbe-next")
    if args.wifi_txbe_next and (args.action != "stage" or not args.wifi_txbe_start):
        parser.error("--wifi-txbe-next requires stage --wifi-txbe-start")
    if args.wifi_txbe_start and (args.action != "stage" or not args.wifi_rxc_tail):
        parser.error("--wifi-txbe-start requires stage --wifi-rxc-tail")
    if args.wifi_rxc_tail and (args.action != "stage" or not args.wifi_rxc_start):
        parser.error("--wifi-rxc-tail requires stage --wifi-rxc-start")
    if args.wifi_rxc_start and (args.action != "stage" or not args.wifi_agc_tail):
        parser.error("--wifi-rxc-start requires stage --wifi-agc-tail")
    if args.wifi_agc_tail and (args.action != "stage" or not args.wifi_agc_next):
        parser.error("--wifi-agc-tail requires stage --wifi-agc-next")
    if args.wifi_agc_next and (args.action != "stage" or not args.wifi_agc_more):
        parser.error("--wifi-agc-next requires stage --wifi-agc-more")
    if args.wifi_agc_more and (args.action != "stage" or not args.wifi_compact):
        parser.error("--wifi-agc-more requires stage --wifi-compact")
    if args.wifi_compact and (args.action != "stage" or not args.wifi_bbp_batch):
        parser.error("--wifi-compact requires stage --wifi-bbp-batch")
    if args.wifi_bbp_batch and (args.action != "stage" or not args.wifi_bbp_agc):
        parser.error("--wifi-bbp-batch requires stage --wifi-bbp-agc")
    if args.wifi_bbp_agc and (args.action != "stage" or not args.wifi_bbp_front):
        parser.error("--wifi-bbp-agc requires stage --wifi-bbp-front")
    if args.wifi_bbp_front and (args.action != "stage" or not args.wifi_bbp_core):
        parser.error("--wifi-bbp-front requires stage --wifi-bbp-core")
    if args.wifi_bbp_core and (args.action != "stage" or not args.wifi_bbp_ready):
        parser.error("--wifi-bbp-core requires stage --wifi-bbp-ready")
    if args.wifi_bbp_ready and (args.action != "stage" or not args.wifi_mac_format):
        parser.error("--wifi-bbp-ready requires stage --wifi-mac-format")
    if args.wifi_mac_format and (args.action != "stage" or not args.wifi_mac_basics):
        parser.error("--wifi-mac-format requires stage --wifi-mac-basics")
    if args.wifi_mac_basics and (args.action != "stage" or not args.wifi_timing_probe):
        parser.error("--wifi-mac-basics requires stage --wifi-timing-probe")
    if args.wifi_timing_probe and (args.action != "stage" or not args.wifi_pair_probe):
        parser.error("--wifi-timing-probe requires stage --wifi-pair-probe")
    if args.wifi_pair_probe and (args.action != "stage" or not args.wifi_write_probe):
        parser.error("--wifi-pair-probe requires stage --wifi-write-probe")
    if args.wifi_write_probe and (args.action != "stage" or not args.wifi_radio_reset):
        parser.error("--wifi-write-probe requires stage --wifi-radio-reset")
    if args.wifi_radio_reset and (args.action != "stage" or not args.wifi_mcu_probe):
        parser.error("--wifi-radio-reset requires stage --wifi-mcu-probe")
    if args.wifi_mcu_probe and (args.action != "stage" or not args.wifi_mac_startup or not args.wifi_firmware_package):
        parser.error("--wifi-mcu-probe requires stage --wifi-firmware-package --wifi-mac-startup")
    if args.wifi_mac_startup and (args.action != "stage" or not args.wifi_firmware_package):
        parser.error("--wifi-mac-startup requires stage --wifi-firmware-package")
    if args.hold_on_failure and (args.action != "stage" or not args.laptop_probe_volume):
        parser.error("--hold-on-failure requires stage --laptop-probe-volume")
    if (args.idle_capture or args.aml_capture or args.desktop_idle_capture or args.battery_capture or args.battery_transitions or args.battery_service or args.battery_usb_read or args.battery_usb_write or args.battery_filesystem or args.battery_desktop or args.battery_text or args.battery_load or args.battery_app_files or args.battery_controls or args.battery_query or args.battery_smbus or args.battery_smbus_files or args.battery_home or args.battery_home_session or args.keyboard_capture or args.cpu_load_capture or args.wifi_capture or args.wifi_calibration or args.wifi_firmware_package) and (args.action != "stage" or not args.laptop_probe_volume):
        parser.error("capture requires stage --laptop-probe-volume")
    if args.power_telemetry and (args.action != "stage" or not args.desktop_idle_capture):
        parser.error("--power-telemetry requires stage --desktop-idle-capture")
    if args.laptop_probe_volume and args.action != "stage":
        parser.error("--laptop-probe-volume applies only to stage")
    if args.keep_previous and args.action != "install":
        parser.error("--keep-previous applies only to install")
    if args.release and args.action != "install":
        parser.error("--release applies only to install")
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
            stage(args.diagnostic, desktop_volume=args.desktop_volume, laptop_probe_volume=args.laptop_probe_volume, idle_capture=args.idle_capture, aml_capture=args.aml_capture, desktop_idle_capture=args.desktop_idle_capture, power_telemetry=args.power_telemetry, battery_capture=args.battery_capture, battery_transitions=args.battery_transitions, battery_service=args.battery_service, hold_on_failure=args.hold_on_failure, battery_usb_read=args.battery_usb_read, battery_usb_write=args.battery_usb_write, battery_filesystem=args.battery_filesystem, battery_desktop=args.battery_desktop, battery_text=args.battery_text, battery_load=args.battery_load, battery_app_files=args.battery_app_files, battery_controls=args.battery_controls, battery_query=args.battery_query, battery_smbus=args.battery_smbus, battery_smbus_files=args.battery_smbus_files, battery_home=args.battery_home, battery_home_session=args.battery_home_session, keyboard_capture=args.keyboard_capture, cpu_load_capture=args.cpu_load_capture, desktop_scale=args.desktop_scale, desktop_brightness=args.desktop_brightness, desktop_audio=args.desktop_audio, desktop_sensors=args.desktop_sensors, desktop_network=args.desktop_network, wifi_capture=args.wifi_capture, wifi_calibration=args.wifi_calibration, wifi_firmware_package=args.wifi_firmware_package, wifi_mac_startup=args.wifi_mac_startup, wifi_mcu_probe=args.wifi_mcu_probe, wifi_radio_reset=args.wifi_radio_reset, wifi_write_probe=args.wifi_write_probe, wifi_pair_probe=args.wifi_pair_probe, wifi_timing_probe=args.wifi_timing_probe, wifi_mac_basics=args.wifi_mac_basics, wifi_mac_format=args.wifi_mac_format, wifi_bbp_ready=args.wifi_bbp_ready, wifi_bbp_core=args.wifi_bbp_core, wifi_bbp_front=args.wifi_bbp_front, wifi_bbp_agc=args.wifi_bbp_agc, wifi_bbp_batch=args.wifi_bbp_batch, wifi_compact=args.wifi_compact, wifi_agc_more=args.wifi_agc_more, wifi_agc_next=args.wifi_agc_next, wifi_agc_tail=args.wifi_agc_tail, wifi_rxc_start=args.wifi_rxc_start, wifi_rxc_tail=args.wifi_rxc_tail, wifi_txbe_start=args.wifi_txbe_start, wifi_txbe_next=args.wifi_txbe_next, wifi_txbe_observe=args.wifi_txbe_observe, wifi_txbe_first=args.wifi_txbe_first, wifi_txbe_second=args.wifi_txbe_second, wifi_txbe_access=args.wifi_txbe_access, wifi_txbe_direct=args.wifi_txbe_direct, wifi_clock=args.wifi_clock, wifi_clock_direct=args.wifi_clock_direct, wifi_clock_pair=args.wifi_clock_pair, wifi_bbp_table=args.wifi_bbp_table, wifi_mac_snapshot=args.wifi_mac_snapshot, wifi_mac_raw=args.wifi_mac_raw, wifi_mac_route=args.wifi_mac_route, wifi_mac_policy=args.wifi_mac_policy, wifi_mac_policy_observe=args.wifi_mac_policy_observe, wifi_mac_policy_validate=args.wifi_mac_policy_validate, wifi_mac_protect=args.wifi_mac_protect, wifi_mac_contention=args.wifi_mac_contention, wifi_mac_txsw=args.wifi_mac_txsw, wifi_mac_beacon=args.wifi_mac_beacon)
        else:
            install(args.release, keep_previous=args.keep_previous)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"hardware-boot: {error}\n")


if __name__ == "__main__":
    main()
