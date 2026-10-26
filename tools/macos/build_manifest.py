#!/usr/bin/env python3
"""Record successful ordinary-image builds; a launcher must verify these hashes."""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess

REPO = Path(__file__).resolve().parents[2]
INPUTS = ("build/kernel/srvros.elf", "build/initramfs.tar", "build/srvros.exfat")


def image_hashes(repo):
    result = {}
    for name in INPUTS:
        with (repo / name).open("rb") as stream:
            result[name] = hashlib.file_digest(stream, "sha256").hexdigest()
    return result


def main():
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=REPO)
    record = {
        "schema": 1,
        "built_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source": git("rev-parse", "HEAD").decode().strip(),
        "worktree_dirty": bool(git("status", "--porcelain")),
        "tracked_diff_sha256": hashlib.sha256(git("diff", "HEAD", "--binary")).hexdigest(),
        "hashes": image_hashes(REPO),
    }
    (REPO / "build/macos-build.json").write_text(json.dumps(record, indent=2) + "\n")
    print("macOS: recorded matched build in build/macos-build.json")


if __name__ == "__main__":
    main()
