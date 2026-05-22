param(
  [string]$Distro = "",
  [string]$NativePath = "~/srvros-node-probe",
  [string]$Sysroot = "build\sysroot\srvros",
  [string]$OutDir = "build\node-srvros-link-probe",
  [switch]$FailOnUnresolved
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -Scope Global -ErrorAction SilentlyContinue) {
  $global:PSNativeCommandUseErrorActionPreference = $false
}

function Quote-RspArg([string]$Value) {
  '"' + ($Value -replace '"', '\"') + '"'
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")
$sysrootPath = Join-Path $repoRoot $Sysroot
$outPath = Join-Path $repoRoot $OutDir
$zig = Join-Path $repoRoot "build\tooling\zig\zig.exe"
$make = "make"

& $make -C $repoRoot srvros-sysroot

if (!(Test-Path -LiteralPath $zig)) {
  throw "Missing Zig toolchain at $zig"
}

$wslArgs = @()
if ($Distro.Length -gt 0) {
  $wslArgs += @("-d", $Distro)
}

$nativeRoot = (& wsl.exe @wslArgs --cd "$repoRoot" -- bash -lc "cd $NativePath && pwd").Trim()
$releaseUnix = "$nativeRoot/ports/upstream/node/out/out/Release"
$releaseWin = (& wsl.exe @wslArgs --cd "$nativeRoot" -- bash -lc "wslpath -w '$releaseUnix'").Trim()

$queryCommand = "build/wsl-tools/ninja-linux/ninja -C ports/upstream/node/out/out/Release -t query node"
$query = & wsl.exe @wslArgs --cd "$nativeRoot" -- bash -lc $queryCommand
if ($LASTEXITCODE -ne 0) {
  throw "Unable to query Node ninja graph. Run probe-wsl-native.ps1 -ProbeTarget node first."
}

$inputs = @()
foreach ($line in $query) {
  $item = $line.Trim()
  if ($item -match '^(obj/.*\.(o|a))$') {
    $inputs += $matches[1]
  }
}
$inputs = $inputs | Select-Object -Unique
if ($inputs.Count -eq 0) {
  throw "Node ninja query returned no object/archive inputs."
}

New-Item -ItemType Directory -Force $outPath | Out-Null
$rsp = Join-Path $outPath "node-srvros-link.rsp"
$log = Join-Path $outPath "node-srvros-link.log"
$stdoutLog = Join-Path $outPath "node-srvros-link.stdout.log"
$stderrLog = Join-Path $outPath "node-srvros-link.stderr.log"
$elf = Join-Path $outPath "node-srvros.elf"
$unresolved = Join-Path $outPath "unresolved-symbols.txt"

$crt0 = Join-Path $sysrootPath "lib\crt0.o"
$appLd = Join-Path $sysrootPath "lib\app.ld"
$srvrosLib = Join-Path $sysrootPath "lib\libsrvros.a"

$rspArgs = @(
  "-m", "elf_x86_64",
  "-nostdlib",
  "-static",
  "-z", "max-page-size=0x1000",
  "--gc-sections",
  "--no-undefined",
  "--error-limit=200",
  "-T", $appLd,
  $crt0
)

$wholeArchive = @(
  "obj/libnode.a",
  "obj/tools/v8_gypfiles/libv8_base_without_compiler.a",
  "obj/deps/zlib/libzlib.a",
  "obj/deps/uv/libuv.a",
  "obj/tools/v8_gypfiles/libv8_snapshot.a"
)

$rspArgs += "--start-group"
foreach ($input in $inputs) {
  $full = Join-Path $releaseWin ($input -replace '/', '\')
  if ($wholeArchive -contains $input) {
    $rspArgs += "--whole-archive"
    $rspArgs += $full
    $rspArgs += "--no-whole-archive"
  } else {
    $rspArgs += $full
  }
}
$rspArgs += $srvrosLib
$rspArgs += "--end-group"
$rspArgs += @("-o", $elf)

($rspArgs | ForEach-Object { Quote-RspArg $_ }) | Set-Content -LiteralPath $rsp

Write-Host "Linking Node object graph against srvros sysroot..."
Write-Host "  inputs: $($inputs.Count)"
Write-Host "  rsp: $rsp"

$process = Start-Process `
  -FilePath $zig `
  -ArgumentList @("ld.lld", "@$rsp") `
  -NoNewWindow `
  -Wait `
  -PassThru `
  -RedirectStandardOutput $stdoutLog `
  -RedirectStandardError $stderrLog
$exitCode = $process.ExitCode
$output = @()
if (Test-Path -LiteralPath $stdoutLog) {
  $output += Get-Content -LiteralPath $stdoutLog
}
if (Test-Path -LiteralPath $stderrLog) {
  $output += Get-Content -LiteralPath $stderrLog
}
$output | Tee-Object -FilePath $log

$symbols = $output |
  Select-String -Pattern 'undefined symbol: (.+)$' |
  ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() } |
  Sort-Object -Unique

if ($symbols.Count -gt 0) {
  $symbols | Set-Content -LiteralPath $unresolved
  Write-Host "Captured $($symbols.Count) unresolved symbols in $unresolved"
} elseif (Test-Path -LiteralPath $unresolved) {
  Remove-Item -LiteralPath $unresolved
}

if ($exitCode -eq 0) {
  Write-Host "srvros Node link probe produced: $elf"
  exit 0
}

Write-Host "srvros Node link probe stopped at the expected link frontier. See $log"
if ($FailOnUnresolved) {
  exit $exitCode
}
exit 0
