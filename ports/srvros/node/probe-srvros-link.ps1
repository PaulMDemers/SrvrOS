param(
  [string]$Distro = "",
  [string]$NativePath = "~/srvros-node-probe",
  [string]$Sysroot = "build\sysroot\srvros",
  [string]$OutDir = "build\node-srvros-link-probe",
  [string]$CompileProbeDir = "build\node-srvros-compile-probe",
  [string]$LibcxxProbeDir = "build\node-srvros-libcxx-probe",
  [switch]$FailOnUnresolved
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -Scope Global -ErrorAction SilentlyContinue) {
  $global:PSNativeCommandUseErrorActionPreference = $false
}

function Quote-RspArg([string]$Value) {
  '"' + ($Value -replace '"', '\"') + '"'
}

function Read-ReplacementManifest([string]$CompileProbePath) {
  $replacements = @{}
  $manifest = Join-Path $CompileProbePath "replacements.tsv"
  if (Test-Path -LiteralPath $manifest) {
    $rows = Import-Csv -LiteralPath $manifest -Delimiter "`t"
    foreach ($row in $rows) {
      if ($row.object -and $row.srvros_object -and
          (Test-Path -LiteralPath $row.srvros_object)) {
        $replacements[$row.object] = $row.srvros_object
      }
    }
  }

  $fallback = @{
    "obj/src/node.node_main.o" = Join-Path $CompileProbePath "node.node_main.srvros.o"
    "obj/src/node.node_snapshot_stub.o" = Join-Path $CompileProbePath "node.node_snapshot_stub.srvros.o"
  }
  foreach ($entry in $fallback.GetEnumerator()) {
    if (!$replacements.ContainsKey($entry.Key) -and
        (Test-Path -LiteralPath $entry.Value)) {
      $replacements[$entry.Key] = $entry.Value
    }
  }

  return $replacements
}

function Quote-BashArg([string]$Value) {
  "'" + ($Value -replace "'", "'\''") + "'"
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")
$sysrootPath = Join-Path $repoRoot $Sysroot
$outPath = Join-Path $repoRoot $OutDir
$compileProbePath = Join-Path $repoRoot $CompileProbeDir
$libcxxProbePath = Join-Path $repoRoot $LibcxxProbeDir
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
$libcxxProbeLib = Join-Path $libcxxProbePath "libsrvros-libcxx-probe.a"

$rspArgs = @(
  "-m", "elf_x86_64",
  "-nostdlib",
  "-static",
  "-z", "max-page-size=0x1000",
  "--gc-sections",
  "--no-undefined",
  "--error-limit=200",
  "-T", $appLd,
  "--defsym=main=_Z4mainiPPc",
  $crt0
)

$wholeArchive = @(
  "obj/libnode.a",
  "obj/tools/v8_gypfiles/libv8_base_without_compiler.a",
  "obj/deps/zlib/libzlib.a",
  "obj/deps/uv/libuv.a",
  "obj/tools/v8_gypfiles/libv8_snapshot.a"
)

$srvrosObjectReplacements = Read-ReplacementManifest $compileProbePath
$directReplacements = @($srvrosObjectReplacements.Keys | Where-Object { $inputs -contains $_ })
$archiveReplacements = @($srvrosObjectReplacements.Keys | Where-Object { !($inputs -contains $_) })
$filteredArchives = @{}
if ($archiveReplacements.Count -gt 0) {
  $wholeArchive = @($wholeArchive | Where-Object { $_ -ne "obj/libnode.a" })

  $filteredLibnode = Join-Path $outPath "libnode.filtered.a"
  $filteredLibnodeArg = $filteredLibnode -replace "'", "'\''"
  $filteredLibnodeUnix = (& wsl.exe @wslArgs --cd "$nativeRoot" -- bash -lc "wslpath -u '$filteredLibnodeArg'").Trim()
  if ($LASTEXITCODE -ne 0 -or $filteredLibnodeUnix.Length -eq 0) {
    throw "Unable to translate filtered archive path $filteredLibnode"
  }
  $deleteListUnix = "$filteredLibnodeUnix.delete-list"
  $keepListUnix = "$filteredLibnodeUnix.keep-list"
  $deleteLines = ($archiveReplacements -join "`n") -replace "'", "'\''"
  $filterCommand = @"
cd $(Quote-BashArg $releaseUnix) &&
cat > $(Quote-BashArg $deleteListUnix) <<'EOF'
$deleteLines
EOF
ar t obj/libnode.a | grep -vxF -f $(Quote-BashArg $deleteListUnix) > $(Quote-BashArg $keepListUnix) &&
rm -f $(Quote-BashArg $filteredLibnodeUnix) &&
xargs -a $(Quote-BashArg $keepListUnix) ar rcs $(Quote-BashArg $filteredLibnodeUnix)
"@
  & wsl.exe @wslArgs --cd "$nativeRoot" -- bash -lc $filterCommand
  if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $filteredLibnode)) {
    throw "Unable to create filtered libnode archive at $filteredLibnode"
  }
  $filteredArchives["obj/libnode.a"] = $filteredLibnode
}

$rspArgs += "--start-group"
foreach ($replacement in $archiveReplacements) {
  $rspArgs += $srvrosObjectReplacements[$replacement]
}
if (Test-Path -LiteralPath $libcxxProbeLib) {
  $rspArgs += $libcxxProbeLib
}
foreach ($input in $inputs) {
  $full = Join-Path $releaseWin ($input -replace '/', '\')
  if ($filteredArchives.ContainsKey($input)) {
    $full = $filteredArchives[$input]
  }
  if ($srvrosObjectReplacements.ContainsKey($input) -and
      (Test-Path -LiteralPath $srvrosObjectReplacements[$input])) {
    $full = $srvrosObjectReplacements[$input]
  }
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
Write-Host "  direct replacements: $($directReplacements.Count)"
Write-Host "  archive replacements: $($archiveReplacements.Count)"
if (Test-Path -LiteralPath $libcxxProbeLib) {
  Write-Host "  libc++ probe: $libcxxProbeLib"
}
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
