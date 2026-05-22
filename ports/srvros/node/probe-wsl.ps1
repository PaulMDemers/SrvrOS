param(
  [string]$Distro = "",
  [string]$ProbeTarget = "node",
  [switch]$NoDownload
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")
$ninjaDir = Join-Path $repoRoot "build\wsl-tools\ninja-linux"
$ninjaExe = Join-Path $ninjaDir "ninja"
$ninjaZip = Join-Path $repoRoot "build\wsl-tools\ninja-linux.zip"

if (!(Test-Path -LiteralPath $ninjaExe)) {
  if ($NoDownload) {
    throw "Missing $ninjaExe and -NoDownload was specified."
  }

  New-Item -ItemType Directory -Force (Split-Path -Parent $ninjaZip) | Out-Null
  Invoke-WebRequest `
    -Uri "https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-linux.zip" `
    -OutFile $ninjaZip
  Expand-Archive -Force $ninjaZip $ninjaDir
}

$wslArgs = @()
if ($Distro.Length -gt 0) {
  $wslArgs += @("-d", $Distro)
}

$wslRoot = (& wsl.exe @wslArgs --cd "$repoRoot" -- bash -lc "pwd").Trim()
$runner = Join-Path $repoRoot "build\wsl-tools\node-probe-runner.sh"
$runnerText = @'
#!/usr/bin/env sh
set -eu

chmod +x build/wsl-tools/ninja-linux/ninja
export PATH="$PWD/build/wsl-tools/ninja-linux:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export NODE_PROBE_TARGET="__NODE_PROBE_TARGET__"

if grep -q "srvros" ports/upstream/node/configure.py; then
  echo "Node srvros patch queue already applied"
else
  ports/srvros/node/apply-patches.sh
fi

ports/srvros/node/probe-linux.sh
'@
$runnerText = $runnerText.Replace("__NODE_PROBE_TARGET__", $ProbeTarget)
Set-Content -LiteralPath $runner -Value $runnerText -NoNewline

Write-Host "Running Node srvros probe in WSL at $wslRoot"
& wsl.exe @wslArgs --cd "$repoRoot" -- bash build/wsl-tools/node-probe-runner.sh
