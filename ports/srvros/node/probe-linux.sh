#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../../.." && pwd)
node_dir="$repo_root/ports/upstream/node"
: "${NINJA:=ninja}"
: "${NINJA_FLAGS:=-j2}"
: "${NODE_PROBE_TARGET:=node}"

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

case "$NODE_PROBE_TARGET" in
  libuv)
    build_target="obj/deps/uv/libuv.a"
    ;;
  libuv-host)
    build_target="obj.host/deps/uv/libuv.a"
    ;;
  v8-base)
    build_target="v8_libbase"
    ;;
  mksnapshot)
    build_target="mksnapshot"
    ;;
  node)
    build_target="node"
    ;;
  *)
    build_target="$NODE_PROBE_TARGET"
    ;;
esac

"$NINJA" $NINJA_FLAGS -C out/out/Release "$build_target"
