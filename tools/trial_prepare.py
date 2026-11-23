"""Prepare the host handoff for an already installed, qualified Retina release."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

# The privileged inspection subprocess uses isolated Python (-I).
if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent))
import hardware_boot as boot
import hardware_desktop as desktop
import hardware_log_archive as logs
import hardware_test as host
from check_exfat_allocation import select_partition
from exfat_test_image import check_clean_image
from trial_qualify import CASES, RESULT, validate_vm, write_json

PROOF = "retina-fractional-proof.txt"
TOPIC = "retina-trial"
CHECKLIST = [
    "Keep the workspace connected. In Settings > Display, record the starting scale at 2880 × 1800.",
    "Apply 1.25×, check readability and pointer alignment, and explicitly revert within 15 seconds.",
    "Apply 1.5× and let it time out. Check restoration of the initial scale and window positions.",
    "Keep 1.75× and wait for the save confirmation. Check menus, Dock, edge controls, scrolling in Help and Files, and dragging, resizing, maximizing and restoring Text. Look for clipping, blur or pointer jumps.",
    "Record the results in a new Text file at /work/srvros-work/{proof}, reopen it, then save a screenshot at 1.75×.",
    "Restore and Keep the original scale, save open work, then choose SrvrOS > Return to console.",
]


def require(value, message):
    if not value:
        raise ValueError(message)


def qualification(path):
    receipt = json.loads(path.read_bytes())
    require((receipt["format"], receipt["recipe"], receipt["status"]) ==
            ("srvros-trial-qualification-v1", "retina", "passed"), "Need a passed Retina qualification")
    staged = json.loads((path.parent / "stage.json").read_bytes())
    require(all(staged[k] == receipt[k] for k in ("release", "manifest", "manifest_sha256")),
            "Stage receipt disagrees with qualification")
    release, manifest = staged["release"], staged["manifest"]
    # The v1 stage writer emits indented JSON and a trailing newline. This
    # binds the saved hash map to the VM receipts even after a tmpfs stage expires.
    encoded = (json.dumps(manifest, indent=2) + "\n").encode()
    require(hashlib.sha256(encoded).hexdigest() == staged["manifest_sha256"], "Qualified manifest digest mismatch")
    volume = host.volume_id(receipt["workspace_uuid"])
    require(boot.RELEASE_RE.fullmatch(release) and manifest["release"] == release and
            manifest["config_version"] == f"ram-usb-v1-retina-desktop-{volume}", "Wrong recipe, volume or release")
    require(set(manifest["sha256"]) == set(boot.FILES) and
            all(re.fullmatch(r"[0-9a-f]{64}", h) for h in manifest["sha256"].values()), "Invalid payload hashes")
    inputs = {k: v for k, v in manifest["sha256"].items() if not k.endswith("limine.conf")}
    require(boot.release_id(inputs, manifest["config_version"]) == release, "Qualified release ID mismatch")
    for case in CASES:
        result = json.loads((path.parent / case / RESULT).read_bytes())
        validate_vm(case, result, staged)
        require(result == receipt["cases"][case], f"Saved {case} VM result disagrees with qualification")
    serial = path.parent / "present/build/review/hardware-grub-serial.log"
    with serial.open("rb") as stream:
        require(any(line.rstrip(b"\r\n") == b"deskhelp: page=retina-trial" for line in stream),
                "Workspace-present VM did not record the opening Retina Help topic")
    return receipt


def boot_selection(release, previous):
    cfg = boot.GRUB_CFG.read_text()
    host.check_ubuntu_default(Path("/etc/default/grub").read_text(), cfg)
    esp_uuid = subprocess.check_output(["findmnt", "-nr", "-M", str(boot.ESP), "-o", "UUID"], text=True).strip()
    require(boot.grub_config(esp_uuid, release, previous) in cfg, "Generated GRUB release entries differ from handoff")
    env = subprocess.check_output(["grub-editenv", "/boot/grub/grubenv", "list"], text=True).splitlines()
    require(not any(line.startswith("next_entry=") and line != "next_entry=" for line in env),
            "GRUB already has a pending one-shot selection")
    firmware = subprocess.check_output(["efibootmgr"], text=True).splitlines()
    require(not any(line.startswith("BootNext:") for line in firmware), "Firmware BootNext is already set")
    order, = [line for line in firmware if line.startswith("BootOrder:")]
    return dict(ubuntu_default_verified=True, firmware_order=order, next_entry=None, boot_next=None)


def inspect_payloads(release, previous, archive):
    require(boot.RELEASE_RE.fullmatch(release) and boot.RELEASE_RE.fullmatch(previous) and previous != release,
            "Expected distinct candidate and accepted recovery releases")
    require(boot.ESP.is_mount(), "Expected the mounted Ubuntu EFI system partition")
    state = json.loads((boot.BASE / "state.json").read_bytes())
    require((state["current"], state["previous"]) == (release, previous), "Installed release pair differs from handoff")
    installed = boot.verify_files(boot.BASE / release, release)
    require(archive.stat().st_dev != boot.BASE.stat().st_dev, "Recovery archive is on the ESP")
    require(boot.verify_files(boot.BASE / previous, previous) == boot.verify_files(archive / previous, previous),
            "Installed recovery differs from its archived copy")
    digest = boot.digest(boot.BASE / previous / "manifest.json")
    require(digest == boot.digest(archive / previous / "manifest.json"), "Recovery manifest bytes differ")
    return installed, digest


def workspace_baseline(mount, volume):
    capacity = logs.require_headroom(mount)
    work = mount / "srvros-work"
    require((work / ".workspace").read_bytes() == f"SRVROS-WORKSPACE-V1 {volume}\n".encode(), "Workspace marker mismatch")
    sessions = []
    if (work / "Logs").exists():
        sessions = sorted(p.name for p in (work / "Logs").iterdir() if p.is_dir() and desktop.SESSION.fullmatch(p.name))
    files = {p.relative_to(work).as_posix(): {"size": p.stat().st_size, "sha256": boot.digest(p)}
             for p in sorted(work.rglob("*")) if p.is_file()}
    return dict(uuid=volume, sessions=sessions, files=files, capacity=capacity,
                root_directories=sorted(p.name for p in work.iterdir() if p.is_dir()))


def inspect_workspace(volume):
    inventory = json.loads(subprocess.check_output([
        "lsblk", "--json", "--tree", "-o", "PATH,TYPE,TRAN,UUID,FSTYPE,MOUNTPOINTS,PTTYPE"], text=True))
    device = Path(select_partition(inventory, volume))
    require(device.is_block_device(), "Expected an unmounted USB block partition")
    table = subprocess.check_output(["lsblk", "-dn", "-o", "PTTYPE", str(device)], text=True).strip()
    require(table == "gpt", "Expected the GPT workspace used by the Retina recipe")
    check_clean_image(device)
    with tempfile.TemporaryDirectory(prefix="srvros-preflight-") as tmp:
        subprocess.run(["mount", "-t", "exfat", "-o", "ro,nosuid,nodev,noexec", str(device), tmp], check=True)
        try:
            baseline = workspace_baseline(Path(tmp), volume)
        finally:
            subprocess.run(["umount", tmp], check=True)
    return dict(baseline, device=str(device), partition_table=table, fsck_clean=True, read_only=True)


def inspect_host(release, volume, previous, archive):
    require(Path("/sys/class/dmi/id/product_name").read_text().strip() == "MacBookPro11,2",
            "Expected the MacBookPro11,2 Retina unit")
    prior_boot_id = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    before = boot_selection(release, previous)
    installed, recovery_hash = inspect_payloads(release, previous, archive)
    baseline = inspect_workspace(host.volume_id(volume))
    require(boot_selection(release, previous) == before, "Boot selection changed during inspection")
    state = json.loads((boot.BASE / "state.json").read_bytes())
    require((state["current"], state["previous"]) == (release, previous), "Installed selection changed during inspection")
    return dict(model="MacBookPro11,2", prior_boot_id=prior_boot_id, baseline=baseline,
                installed=installed, recovery_manifest_sha256=recovery_hash, **before)


def prepare(path, previous, archive, parent, use_sudo):
    path = path.resolve(strict=True)
    archive = archive.resolve()
    q = qualification(path)
    volume, release = host.volume_id(q["workspace_uuid"]), q["release"]
    require(boot.RELEASE_RE.fullmatch(previous), "Invalid accepted recovery release")
    require(all(not archive.is_relative_to(root / "build") for root in (boot.ROOT, Path(q["source_checkout"]))),
            "Accepted recovery archive must survive cleaning the build tree")
    if use_sudo:
        sudo = ["sudo"] if sys.stdin.isatty() else ["sudo", "-n"]
        command = [*sudo, "--", sys.executable, "-I", "-B", str(Path(__file__).resolve()),
                   release, volume, previous, str(archive)]
        inspected = json.loads(subprocess.check_output(command))
    else:
        inspected = inspect_host(release, volume, previous, archive)
    installed = inspected.pop("installed")
    require(installed["config_version"] == q["manifest"]["config_version"] and
            installed["sha256"] == q["manifest"]["sha256"], "Installed bytes differ from the VM-qualified payload")
    baseline = inspected.pop("baseline")
    existing = {name.casefold() for name in (*baseline["files"], *baseline["root_directories"])}
    proof = PROOF
    number = 2
    while proof.casefold() in existing:
        proof = f"{Path(PROOF).stem}-{number}.txt"
        number += 1
    conflict = proof != PROOF
    parent.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="retina-", dir=parent.resolve()))
    run = dict(format="srvros-trial-preflight-v1", phase="checklist_handoff_required" if conflict else "verified_installed_unarmed",
               source=installed["git_head"], qualification_source=q["manifest"]["git_head"], release=release,
               qualification=str(path), qualification_manifest_sha256=q["manifest_sha256"], workspace_uuid=volume,
               recovery_release=previous, recovery_archive=str(archive), guest_help=TOPIC, checklist=[item.format(proof=proof) for item in CHECKLIST],
               proof_file=proof, expected_receipts=["retina-scales:", "exit.log"],
               proof_file_preexisting=conflict, checked_at=datetime.now(timezone.utc).isoformat(), **inspected)
    write_json(folder / "workspace-before.json", baseline)
    # Publish the collector entry point last, after the complete baseline.
    artifact = folder / "run.json"
    if conflict:
        run["reason"] = (f"Guest Help names an existing {PROOF}. Before any attended reboot, deliver the "
                         f"complete saved checklist using {proof} and confirm the operator captured it.")
    write_json(artifact, run)
    return artifact, run


if __name__ == "__main__":
    p = argparse.ArgumentParser(description="Inspect Retina handoff inputs read-only")
    for name in ("release", "volume", "previous", "archive"):
        p.add_argument(name)
    a = p.parse_args()
    try:
        print(json.dumps(inspect_host(a.release, a.volume, a.previous, Path(a.archive))))
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as exc:
        sys.exit(str(exc))
