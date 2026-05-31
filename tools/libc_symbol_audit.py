#!/usr/bin/env python3
"""Compare srvros public libc-style declarations with libsrvros.a exports."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path


DECL_RE = re.compile(
    r"^\s*(?:extern\s+)?(?:[A-Za-z_][\w\s\*\(\)]*?\s+)?([A-Za-z_]\w*)\s*\([^;{}]*\)\s*;"
)
SKIP_NAMES = {"main", "void"}


def collect_declarations(include_dir: Path) -> dict[str, list[str]]:
    declarations: dict[str, list[str]] = {}
    for header in sorted(include_dir.rglob("*.h")):
        text = header.read_text(encoding="utf-8", errors="ignore")
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        current = ""
        for raw in text.splitlines():
            line = raw.split("//", 1)[0].strip()
            if not line or line.startswith("#") or line.startswith("typedef "):
                continue
            current = f"{current} {line}".strip()
            if ";" not in line:
                continue
            logical = current
            current = ""
            if logical.startswith(("struct ", "union ", "enum ")):
                continue
            match = DECL_RE.match(logical)
            if not match:
                continue
            name = match.group(1)
            if name not in SKIP_NAMES:
                declarations.setdefault(name, []).append(str(header))
    return declarations


def collect_exports(archive: Path, nm: str) -> set[str]:
    proc = subprocess.run(
        [nm, "-g", "--defined-only", str(archive)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr)
        raise SystemExit(proc.returncode)
    exports: set[str] = set()
    for line in proc.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-2] in {"T", "W", "D", "B", "R", "V"}:
            exports.add(parts[-1])
    return exports


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--include", default="userspace/lib/include")
    parser.add_argument("--archive", default="build/userspace/lib/libsrvros.a")
    parser.add_argument("--nm", default=os.environ.get("NM", "nm"))
    parser.add_argument("--fail-missing", action="store_true")
    args = parser.parse_args()

    include_dir = Path(args.include)
    archive = Path(args.archive)
    if not include_dir.exists():
        raise SystemExit(f"missing include directory: {include_dir}")
    if not archive.exists():
        raise SystemExit(f"missing archive: {archive}; build it first")

    declarations = collect_declarations(include_dir)
    exports = collect_exports(archive, args.nm)
    missing = sorted(name for name in declarations if name not in exports)
    print(f"headers: {len(list(include_dir.rglob('*.h')))}")
    print(f"declared functions: {len(declarations)}")
    print(f"exported symbols: {len(exports)}")
    print(f"missing declared exports: {len(missing)}")
    for name in missing:
        locations = ", ".join(sorted(set(declarations[name]))[:2])
        print(f"missing\t{name}\t{locations}")
    return 1 if args.fail_missing and missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
