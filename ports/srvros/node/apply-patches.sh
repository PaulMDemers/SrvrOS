#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../../.." && pwd)
node_dir="$repo_root/ports/upstream/node"

git -C "$node_dir" apply --ignore-space-change --ignore-whitespace \
  "$script_dir/patches/0001-add-srvros-gyp-configure-probe.patch"
