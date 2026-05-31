param(
  [string]$Distro = "",
  [string]$NativePath = "~/srvros-node-probe",
  [switch]$SkipCompile
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")

$wslArgs = @()
if ($Distro.Length -gt 0) {
  $wslArgs += @("-d", $Distro)
}

$mountedRoot = (& wsl.exe @wslArgs --cd "$repoRoot" -- bash -lc "pwd").Trim()
$nativeRoot = (& wsl.exe @wslArgs --cd "$repoRoot" -- bash -lc "cd $NativePath && pwd").Trim()
if ($LASTEXITCODE -ne 0 -or $nativeRoot.Length -eq 0) {
  throw "Native Node probe checkout not found at $NativePath. Run probe-wsl-native.ps1 first."
}

$copyToNative = @"
set -eu
mkdir -p "$nativeRoot/ports/upstream/node/lib/internal/main"
mkdir -p "$nativeRoot/ports/upstream/node/lib/internal"
cp "$mountedRoot/ports/upstream/node/lib/crypto.js" "$nativeRoot/ports/upstream/node/lib/crypto.js"
cp "$mountedRoot/ports/upstream/node/lib/internal/srvros_crypto.js" "$nativeRoot/ports/upstream/node/lib/internal/srvros_crypto.js"
cp "$mountedRoot/ports/upstream/node/lib/timers.js" "$nativeRoot/ports/upstream/node/lib/timers.js"
cp "$mountedRoot/ports/upstream/node/lib/internal/timers.js" "$nativeRoot/ports/upstream/node/lib/internal/timers.js"
cp "$mountedRoot/ports/upstream/node/lib/internal/main/run_main_module.js" "$nativeRoot/ports/upstream/node/lib/internal/main/run_main_module.js"
cp "$mountedRoot/ports/upstream/node/lib/internal/main/eval_string.js" "$nativeRoot/ports/upstream/node/lib/internal/main/eval_string.js"
"@ -replace "`r?`n", "; "
& wsl.exe @wslArgs --cd "$repoRoot" -- bash -lc $copyToNative

$generate = @"
set -eu
export PATH="$nativeRoot/build/wsl-tools/ninja-linux:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
cd "$nativeRoot"
rm -f ports/upstream/node/out/out/Release/gen/node_javascript.cc
build/wsl-tools/ninja-linux/ninja -C ports/upstream/node/out/out/Release gen/node_javascript.cc
"@ -replace "`r?`n", "; "
& wsl.exe @wslArgs --cd "$nativeRoot" -- bash -lc $generate

$copyBack = @"
set -eu
mkdir -p "$mountedRoot/ports/upstream/node/out/out/Release/gen"
cp "$nativeRoot/ports/upstream/node/out/out/Release/gen/node_javascript.cc" "$mountedRoot/ports/upstream/node/out/out/Release/gen/node_javascript.cc"
"@ -replace "`r?`n", "; "
& wsl.exe @wslArgs --cd "$repoRoot" -- bash -lc $copyBack

if (!$SkipCompile) {
  & powershell -ExecutionPolicy Bypass -File (Join-Path $scriptDir "probe-srvros-compile.ps1") `
    -Objects obj/gen/libnode.node_javascript.o `
    -FailOnError
}
