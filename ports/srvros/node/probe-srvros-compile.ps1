param(
  [string]$Distro = "",
  [string]$NativePath = "~/srvros-node-probe",
  [string]$Sysroot = "build\sysroot\srvros",
  [string]$OutDir = "build\node-srvros-compile-probe",
  [switch]$FailOnError
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -Scope Global -ErrorAction SilentlyContinue) {
  $global:PSNativeCommandUseErrorActionPreference = $false
}

function Quote-RspArg([string]$Value) {
  '"' + ($Value -replace '\\', '\\' -replace '"', '\"') + '"'
}

function Invoke-CapturedProcess(
  [string]$FilePath,
  [string[]]$ArgumentList,
  [string]$StdoutPath,
  [string]$StderrPath
) {
  $process = Start-Process `
    -FilePath $FilePath `
    -ArgumentList $ArgumentList `
    -NoNewWindow `
    -Wait `
    -PassThru `
    -RedirectStandardOutput $StdoutPath `
    -RedirectStandardError $StderrPath
  return $process.ExitCode
}

function Read-LogLines([string[]]$Paths) {
  $lines = @()
  foreach ($path in $Paths) {
    if (Test-Path -LiteralPath $path) {
      $lines += Get-Content -LiteralPath $path
    }
  }
  return $lines
}

function Write-UndefinedSymbols([string]$ObjectPath, [string]$OutputPath) {
  if (!(Test-Path -LiteralPath $ObjectPath)) {
    return
  }

  $nm = (Get-Command llvm-nm -ErrorAction SilentlyContinue)
  if ($null -eq $nm) {
    $nm = (Get-Command nm -ErrorAction SilentlyContinue)
  }
  if ($null -eq $nm) {
    return
  }

  $rawSymbols = @(& $nm.Source --undefined-only --format=posix $ObjectPath 2>$null)
  $symbols = @($rawSymbols | ForEach-Object {
    $parts = $_.Trim() -split '\s+'
    if ($parts.Count -ge 2 -and $parts[1] -eq "U") {
      $parts[0]
    }
  } | Sort-Object -Unique)

  if ($symbols.Count -gt 0) {
    $symbols | Set-Content -LiteralPath $OutputPath
  } elseif (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath
  }
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
if (!(Test-Path -LiteralPath $sysrootPath)) {
  throw "Missing srvros sysroot at $sysrootPath"
}

$wslArgs = @()
if ($Distro.Length -gt 0) {
  $wslArgs += @("-d", $Distro)
}

$nativeRoot = (& wsl.exe @wslArgs --cd "$repoRoot" -- bash -lc "cd $NativePath && pwd").Trim()
if ($LASTEXITCODE -ne 0 -or $nativeRoot.Length -eq 0) {
  throw "Unable to resolve WSL Node probe checkout at $NativePath"
}

$nodeRootUnix = "$nativeRoot/ports/upstream/node"
$releaseUnix = "$nodeRootUnix/out/out/Release"
$nodeRootWin = (& wsl.exe @wslArgs --cd "$nativeRoot" -- bash -lc "wslpath -w '$nodeRootUnix'").Trim()
$releaseWin = (& wsl.exe @wslArgs --cd "$nativeRoot" -- bash -lc "wslpath -w '$releaseUnix'").Trim()
$sourceRootWin = Join-Path $repoRoot "ports\upstream\node"
$localReleaseWin = Join-Path $sourceRootWin "out\out\Release"
$generatedRootWin = $releaseWin
if (Test-Path -LiteralPath (Join-Path $localReleaseWin "gen")) {
  $generatedRootWin = $localReleaseWin
}

if (!(Test-Path -LiteralPath $sourceRootWin)) {
  throw "Missing local Node source checkout at $sourceRootWin"
}
if (!(Test-Path -LiteralPath $releaseWin)) {
  throw "Missing Node release build graph at $releaseWin. Run probe-wsl-native.ps1 -ProbeTarget node first."
}

New-Item -ItemType Directory -Force $outPath | Out-Null

$zigLib = Join-Path (Split-Path -Parent $zig) "lib"
$libcxxInclude = Join-Path $zigLib "libcxx\include"
$libcxxAbiInclude = Join-Path $zigLib "libcxxabi\include"
$compilerInclude = Join-Path $zigLib "include"

$defines = @(
  "_GLIBCXX_USE_CXX11_ABI=1",
  "_FILE_OFFSET_BITS=64",
  "NODE_OPENSSL_CONF_NAME=nodejs_conf",
  "ICU_NO_USER_DATA_OVERRIDE",
  "__STDC_FORMAT_MACROS",
  "OPENSSL_NO_PINSHARED",
  "OPENSSL_THREADS",
  'NODE_ARCH="x64"',
  'NODE_PLATFORM="srvros"',
  "NODE_WANT_INTERNALS=1",
  "__POSIX__",
  "NODE_USE_V8_PLATFORM=1",
  "NODE_BUNDLED_ZLIB",
  "NODE_BUNDLED_ZSTD",
  "HAVE_OPENSSL=0",
  "HAVE_AMARO=1",
  "HAVE_SQLITE=0",
  "HAVE_QUIC=0",
  "XXH_NAMESPACE=ZSTD_",
  "ZSTD_MULTITHREAD",
  "ZSTD_DISABLE_ASM",
  "_LARGEFILE_SOURCE",
  "_POSIX_C_SOURCE=200112",
  "NGHTTP2_STATICLIB"
)

$cxxIncludeDirs = @(
  $libcxxInclude,
  $libcxxAbiInclude
)

$includeDirs = @(
  $compilerInclude,
  (Join-Path $sysrootPath "include"),
  (Join-Path $sourceRootWin "src"),
  (Join-Path $generatedRootWin "gen"),
  (Join-Path $generatedRootWin "gen\include"),
  (Join-Path $sourceRootWin "deps\v8\include"),
  (Join-Path $sourceRootWin "deps\postject"),
  (Join-Path $sourceRootWin "deps\histogram\src"),
  (Join-Path $sourceRootWin "deps\histogram\include"),
  (Join-Path $sourceRootWin "deps\nbytes\include"),
  (Join-Path $sourceRootWin "deps\zlib"),
  (Join-Path $sourceRootWin "deps\llhttp\include"),
  (Join-Path $sourceRootWin "deps\cares\include"),
  (Join-Path $sourceRootWin "deps\uv\include"),
  (Join-Path $sourceRootWin "deps\uvwasi\include"),
  (Join-Path $sourceRootWin "deps\nghttp2\lib\includes"),
  (Join-Path $sourceRootWin "deps\ada"),
  (Join-Path $sourceRootWin "deps\merve"),
  (Join-Path $sourceRootWin "deps\simdjson"),
  (Join-Path $sourceRootWin "deps\v8\third_party\simdutf"),
  (Join-Path $sourceRootWin "deps\brotli\c\include"),
  (Join-Path $sourceRootWin "deps\zstd\lib")
) | Where-Object { Test-Path -LiteralPath $_ }

$commonArgs = @(
  "-target", "x86_64-freestanding-none",
  "-ffreestanding",
  "-fno-stack-protector",
  "-fno-stack-check",
  "-fno-lto",
  "-fno-PIC",
  "-ffunction-sections",
  "-fdata-sections",
  "-m64",
  "-mcpu=x86_64",
  "-mabi=sysv",
  "-mno-red-zone",
  "-O2",
  "-g",
  "-std=gnu++20",
  "-nostdinc++",
  "-fno-rtti",
  "-fno-exceptions",
  "-fno-strict-aliasing",
  "-Wall",
  "-Wextra",
  "-Wno-unused-parameter",
  "-Wno-restrict",
  "-Wno-deprecated-declarations",
  "-Wno-invalid-offsetof",
  "-D_LIBCPP_HAS_NO_THREADS",
  "-D_LIBCPP_HAS_WIDE_CHARACTERS=1",
  "-D_LIBCPP_HAS_LOCALIZATION=1",
  "-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE=1",
  "-D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE"
)

foreach ($define in $defines) {
  $commonArgs += "-D$define"
}
foreach ($include in $cxxIncludeDirs) {
  $commonArgs += "-I"
  $commonArgs += $include
}
foreach ($include in $includeDirs) {
  $commonArgs += "-isystem"
  $commonArgs += $include
}

$targets = @(
  @{
    Name = "node_main"
    Source = Join-Path $sourceRootWin "src\node_main.cc"
    LinuxObject = Join-Path $releaseWin "obj\src\node.node_main.o"
    SrvrosObject = Join-Path $outPath "node.node_main.srvros.o"
  },
  @{
    Name = "node_snapshot_stub"
    Source = Join-Path $sourceRootWin "src\node_snapshot_stub.cc"
    LinuxObject = Join-Path $releaseWin "obj\src\node.node_snapshot_stub.o"
    SrvrosObject = Join-Path $outPath "node.node_snapshot_stub.srvros.o"
  }
)

$failed = $false
$compiled = @()

foreach ($target in $targets) {
  $name = $target.Name
  $rsp = Join-Path $outPath "$name.rsp"
  $stdoutLog = Join-Path $outPath "$name.stdout.log"
  $stderrLog = Join-Path $outPath "$name.stderr.log"
  $log = Join-Path $outPath "$name.compile.log"
  $srvrosObject = $target.SrvrosObject

  $args = $commonArgs + @("-c", $target.Source, "-o", $srvrosObject)
  ($args | ForEach-Object { Quote-RspArg $_ }) | Set-Content -LiteralPath $rsp

  Write-Host "Compiling $name for srvros..."
  $exitCode = Invoke-CapturedProcess -FilePath $zig -ArgumentList @("c++", "@$rsp") -StdoutPath $stdoutLog -StderrPath $stderrLog
  $output = Read-LogLines @($stdoutLog, $stderrLog)
  $output | Tee-Object -FilePath $log

  if ($exitCode -eq 0) {
    $compiled += $name
    Write-UndefinedSymbols -ObjectPath $srvrosObject -OutputPath (Join-Path $outPath "$name.srvros.undefined.txt")
    Write-UndefinedSymbols -ObjectPath $target.LinuxObject -OutputPath (Join-Path $outPath "$name.linux.undefined.txt")
    Write-Host "  wrote $srvrosObject"
  } else {
    $failed = $true
    Write-Host "  stopped at compile frontier; see $log"
  }
}

$summary = @(
  "srvros Node compile probe",
  "nativeRoot=$nativeRoot",
  "sourceRoot=$sourceRootWin",
  "generatedRoot=$generatedRootWin",
  "release=$releaseWin",
  "compiled=$($compiled -join ', ')",
  "failed=$failed"
)
$summary | Set-Content -LiteralPath (Join-Path $outPath "summary.txt")

if ($failed) {
  Write-Host "srvros Node compile probe reached the expected compile frontier. See $outPath"
  if ($FailOnError) {
    exit 1
  }
  exit 0
}

Write-Host "srvros Node compile probe completed. See $outPath"
exit 0
