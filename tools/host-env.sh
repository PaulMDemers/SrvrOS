#!/usr/bin/env bash
# Optional repository-local Ubuntu tools, unpacked under build/host-tools/root.
# Usage: tools/host-env.sh make -j2 (normal system tools also work).
set -euo pipefail
repo=$(cd -- "$(dirname -- "$0")/.." && pwd)
host_root="$repo/build/host-tools/root"
if [[ -d "$host_root/usr/bin" ]]; then
    export PATH="$repo/build/host-tools/bin:$host_root/usr/bin:$PATH"
    export LD_LIBRARY_PATH="$host_root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export OVMF_CODE="${OVMF_CODE:-$host_root/usr/share/OVMF/OVMF_CODE_4M.fd}"
    export QEMU_DATA_DIR="${QEMU_DATA_DIR:-$host_root/usr/share/qemu}"
    export QEMU_MODULE_DIR="$host_root/usr/lib/x86_64-linux-gnu/qemu"
fi
exec "$@"
