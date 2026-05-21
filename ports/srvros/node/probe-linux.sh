#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../../.." && pwd)
node_dir="$repo_root/ports/upstream/node"

cd "$node_dir"
python3 configure.py \
  --dest-os=srvros \
  --dest-cpu=x64 \
  --cross-compiling \
  --fully-static \
  --without-npm \
  --without-corepack \
  --without-ssl \
  --without-sqlite \
  --without-inspector \
  --without-intl \
  --without-node-snapshot \
  --without-node-code-cache \
  --v8-lite-mode \
  --ninja

ninja -C out/out/Release node
