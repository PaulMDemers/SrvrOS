"""Stage one Retina payload and collect two independent, exact-image VM results."""
from datetime import datetime, timezone
import hashlib
import inspect
import json
from pathlib import Path
import subprocess
import sys
import tempfile


CASES = ("present", "missing")
RESULT = Path("build/review/hardware-grub-result.json")


def write_json(path, value):
    draft = path.with_suffix(".tmp")
    draft.write_text(json.dumps(value, indent=2) + "\n")
    draft.replace(path)


def source_step(operation, source, stage, run, volume, qemu, release=None):
    # Import the recipe and its dependencies from the selected built checkout.
    # Each invocation has a fresh interpreter, isolated STAGE and output root.
    sys.path.insert(0, str(source / "tools"))
    import hardware_boot as boot
    if boot.ROOT != source:
        raise ValueError("Selected checkout does not supply hardware_boot.py")
    boot.STAGE = stage
    if operation == "stage":
        if "desktop_retina" not in inspect.signature(boot.stage).parameters:
            raise ValueError("Selected checkout lacks the Retina staging recipe")
        release = boot.stage(desktop_retina=True, desktop_volume=volume)
        manifest = boot.verify(stage / release, release)
        if manifest["config_version"] != f"ram-usb-v1-retina-desktop-{volume}":
            raise ValueError("Staged configuration differs from the requested Retina recipe")
        write_json(run / "stage.json", dict(release=release, manifest=manifest,
                   manifest_sha256=boot.digest(stage / release / "manifest.json")))
        return

    import hardware_boot_smoke as smoke
    # The legacy harness reads a latest file. Only use this invocation's
    # private stage, with a pinned release checked on both sides of the VM.
    if (stage / "latest").read_text().strip() != release:
        raise ValueError("Private stage selector differs from the pinned release")
    boot.verify(stage / release, release)
    boot.ROOT = run / operation
    flag = "--retina-fractional" if operation == "present" else "--retina-missing-workspace"
    sys.argv = [smoke.__file__, "--desktop-check", "--workspace-table", "gpt", flag, "--qemu", qemu]
    smoke.main()
    boot.verify(stage / release, release)


def run_step(operation, source, stage, run, volume, qemu, release=None):
    command = [sys.executable, "-E", "-B", str(Path(__file__).resolve()), operation,
               str(source), str(stage), str(run), volume, qemu]
    if release:
        command.append(release)
    log = run / f"{operation}.log"
    with log.open("xb") as output:
        result = subprocess.run(command, cwd=source, stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"{operation} exited {result.returncode}. See {log}")


def validate_vm(case, result, staged):
    if not isinstance(result, dict):
        raise ValueError(f"{case} VM returned an invalid result")
    expected = {"release": staged["release"], "manifest_sha256": staged["manifest_sha256"], "status": "PASS"}
    if case == "present":
        expected.update(retina_desktop=True, retina_fractional=True, workspace_table="gpt",
                        saved=True, unmounted=True, automatic_return=True)
    else:
        expected.update(retina_missing_workspace=True, returned=True)
    for key, value in expected.items():
        if type(result.get(key)) is not type(value) or result[key] != value:
            raise ValueError(f"{case} VM result has missing or mismatched {key}")


def qualify(source, volume, parent, stage_parent, qemu):
    source = source.resolve()
    parent.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="retina-", dir=parent.resolve()))
    path = run / "qualification.json"
    receipt = dict(format="srvros-trial-qualification-v1", recipe="retina", status="running",
                   started_at=datetime.now(timezone.utc).isoformat(), source_checkout=str(source),
                   workspace_uuid=volume, qemu=qemu, cases={case: {"status": "pending"} for case in CASES})
    write_json(path, receipt)
    print(f"Qualification receipt: {path}", file=sys.stderr, flush=True)
    try:
        if stage_parent:
            stage_parent.mkdir(parents=True, exist_ok=True)
            stage = Path(tempfile.mkdtemp(prefix="srvros-retina-stage-", dir=stage_parent.resolve()))
        else:
            stage = run / "stage"
        receipt["stage"] = str(stage)
        run_step("stage", source, stage, run, volume, qemu)
        staged = json.loads((run / "stage.json").read_text())
        receipt.update(staged)
        release = staged["release"]
        print(f"Staged {release}", file=sys.stderr, flush=True)
        for case in CASES:
            receipt["cases"][case] = {"status": "running"}
            write_json(path, receipt)
            try:
                run_step(case, source, stage, run, volume, qemu, release)
                result = json.loads((run / case / RESULT).read_text())
                validate_vm(case, result, staged)
                receipt["cases"][case] = result
            except (OSError, ValueError, RuntimeError) as exc:
                receipt["cases"][case] = {"status": "failed", "error": str(exc)}
            print(f"Workspace {case}: {receipt['cases'][case]['status']}", file=sys.stderr, flush=True)
            write_json(path, receipt)
        if hashlib.sha256((stage / release / "manifest.json").read_bytes()).hexdigest() != staged["manifest_sha256"]:
            raise ValueError("Staged manifest changed during qualification")
        receipt["status"] = "passed" if all(item["status"] == "PASS" for item in receipt["cases"].values()) else "failed"
    except (OSError, ValueError, KeyError, RuntimeError) as exc:
        receipt.update(status="failed", error=str(exc))
    except KeyboardInterrupt:
        receipt.update(status="interrupted")
        raise
    finally:
        receipt["finished_at"] = datetime.now(timezone.utc).isoformat()
        write_json(path, receipt)
    return path, receipt["status"] == "passed"


if __name__ == "__main__":
    operation, source, stage, run, volume, qemu, *pinned = sys.argv[1:]
    source_step(operation, Path(source), Path(stage), Path(run), volume, qemu, *pinned)
