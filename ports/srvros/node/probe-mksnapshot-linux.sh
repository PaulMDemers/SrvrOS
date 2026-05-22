#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
NODE_PROBE_TARGET=mksnapshot exec "$script_dir/probe-linux.sh"
