#!/bin/sh
# Use native host tools while cross-compiling SrvrOS for x86_64.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
if [ "$(uname -sm)" != "Darwin arm64" ]; then
    echo "This wrapper requires Apple Silicon macOS; see docs/development.md." >&2
    exit 1
fi
brew_prefix=$(brew --prefix)
export PATH="$brew_prefix/opt/make/libexec/gnubin:$brew_prefix/opt/gnu-tar/libexec/gnubin:$brew_prefix/bin:$PATH"
export OVMF_CODE="${OVMF_CODE:-$brew_prefix/share/qemu/edk2-x86_64-code.fd}"
export ZIG_GLOBAL_CACHE_DIR="$repo/build/tooling/zig-cache"
export ZIG_LOCAL_CACHE_DIR="$repo/build/tooling/zig-local-cache"
export COPYFILE_DISABLE=1
cd "$repo"
case "$(make --version | head -1)" in
    "GNU Make 4."*) ;;
    *) echo "Install GNU Make 4 with: brew install make" >&2; exit 1 ;;
esac
case "$(tar --version | head -1)" in
    "tar (GNU tar)"*) ;;
    *) echo "Install GNU tar with: brew install gnu-tar" >&2; exit 1 ;;
esac
if [ "$#" -eq 0 ]; then set -- -j2 all; fi
record=0
for arg in "$@"; do
    if [ "$arg" = all ]; then record=1; fi
done
if [ "$record" -eq 1 ]; then
    # A simulated/touch-only build must never relabel old images as new source.
    # MAKEFLAGS short options are commonly stored without a leading dash.
    for arg in "$@" ${MAKEFLAGS-} ${MFLAGS-}; do
        case "$arg" in
            --just-print|--dry-run|--recon|--question|--touch)
                echo "Build manifests require a real build; omit $arg." >&2; exit 1 ;;
            --*|*=*) ;;
            -*)
                case "$arg" in *[nqt]*)
                    echo "Build manifests require a real build; omit $arg." >&2; exit 1 ;;
                esac ;;
        esac
    done
    case "${MAKEFLAGS-}" in
        [!-\ ]*)
            flags=${MAKEFLAGS%% *}
            case "$flags" in *[nqt]*)
                echo "Build manifests require a real build; clear MAKEFLAGS." >&2; exit 1 ;;
            esac ;;
    esac
fi
make "$@"
if [ "$record" -eq 1 ]; then python3 tools/macos/build_manifest.py; fi
