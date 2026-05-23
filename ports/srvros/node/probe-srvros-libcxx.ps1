param(
  [string]$Sysroot = "build\sysroot\srvros",
  [string]$OutDir = "build\node-srvros-libcxx-probe",
  [string]$Sources = "",
  [string]$SourceList = "",
  [switch]$FailOnError
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -Scope Global -ErrorAction SilentlyContinue) {
  $global:PSNativeCommandUseErrorActionPreference = $false
}

function Quote-RspArg([string]$Value) {
  '"' + ($Value -replace '\\', '\\' -replace '"', '\"') + '"'
}

function Split-SourceSpec([string]$Spec) {
  if ($Spec.Length -eq 0) {
    return @()
  }
  return @($Spec -split '[,;\s]+' | Where-Object { $_.Length -gt 0 })
}

function Convert-SourceToName([string]$Source) {
  return (($Source -replace '[\\/]', '_' -replace '\.cpp$', '') -replace '[^A-Za-z0-9_.-]', '_')
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

$zigLib = Join-Path (Split-Path -Parent $zig) "lib"
$libcxxRoot = Join-Path $zigLib "libcxx"
$libcxxSrc = Join-Path $libcxxRoot "src"
$libcxxInclude = Join-Path $libcxxRoot "include"
$libcxxAbiInclude = Join-Path $zigLib "libcxxabi\include"
$compilerInclude = Join-Path $zigLib "include"

$defaultSources = @(
  "string.cpp",
  "memory.cpp",
  "stdexcept.cpp",
  "verbose_abort.cpp",
  "hash.cpp",
  "ios.cpp",
  "ostream.cpp",
  "locale.cpp",
  "typeinfo.cpp",
  "exception.cpp"
)

$requestedSources = @()
$requestedSources += Split-SourceSpec $Sources
if ($SourceList.Length -gt 0) {
  if (!(Test-Path -LiteralPath $SourceList)) {
    throw "Missing libc++ source list at $SourceList"
  }
  $requestedSources += Get-Content -LiteralPath $SourceList |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_.Length -gt 0 -and !($_.StartsWith("#")) }
}
if ($requestedSources.Count -eq 0) {
  $requestedSources = $defaultSources
}
$requestedSources = @($requestedSources | Select-Object -Unique)

New-Item -ItemType Directory -Force $outPath | Out-Null

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
  "-D_LIBCPP_HAS_NO_THREADS",
  "-D_LIBCPP_HAS_WIDE_CHARACTERS=1",
  "-D_LIBCPP_HAS_LOCALIZATION=1",
  "-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE=1",
  "-D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE",
  "-I", $libcxxInclude,
  "-I", $libcxxAbiInclude,
  "-I", $compilerInclude,
  "-isystem", (Join-Path $sysrootPath "include")
)

$failed = $false
$compiled = @()
$failedSources = @()
$objects = @()
$manifest = @("source`tobject")

foreach ($source in $requestedSources) {
  $sourcePath = Join-Path $libcxxSrc ($source -replace '/', '\')
  if (!(Test-Path -LiteralPath $sourcePath)) {
    $failed = $true
    $failedSources += $source
    Write-Host "Missing libc++ source $sourcePath"
    continue
  }

  $name = Convert-SourceToName $source
  $object = Join-Path $outPath "$name.srvros.o"
  $rsp = Join-Path $outPath "$name.rsp"
  $stdoutLog = Join-Path $outPath "$name.stdout.log"
  $stderrLog = Join-Path $outPath "$name.stderr.log"
  $log = Join-Path $outPath "$name.compile.log"

  $args = $commonArgs + @("-c", $sourcePath, "-o", $object)
  ($args | ForEach-Object { Quote-RspArg $_ }) | Set-Content -LiteralPath $rsp

  Write-Host "Compiling libc++ $source for srvros..."
  $exitCode = Invoke-CapturedProcess -FilePath $zig -ArgumentList @("c++", "@$rsp") -StdoutPath $stdoutLog -StderrPath $stderrLog
  $output = @()
  if (Test-Path -LiteralPath $stdoutLog) {
    $output += Get-Content -LiteralPath $stdoutLog
  }
  if (Test-Path -LiteralPath $stderrLog) {
    $output += Get-Content -LiteralPath $stderrLog
  }
  $output | Tee-Object -FilePath $log

  if ($exitCode -eq 0) {
    $compiled += $source
    $objects += $object
    $manifest += "$source`t$object"
    Write-Host "  wrote $object"
  } else {
    $failed = $true
    $failedSources += $source
    Write-Host "  stopped at compile frontier; see $log"
  }
}

$archive = Join-Path $outPath "libsrvros-libcxx-probe.a"
if ($objects.Count -gt 0) {
  if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive
  }
  & $zig ar rcs $archive @objects
  if ($LASTEXITCODE -ne 0) {
    throw "Unable to create $archive"
  }
}

$manifest | Set-Content -LiteralPath (Join-Path $outPath "manifest.tsv")
if ($failedSources.Count -gt 0) {
  $failedSources | Set-Content -LiteralPath (Join-Path $outPath "failed.txt")
} elseif (Test-Path -LiteralPath (Join-Path $outPath "failed.txt")) {
  Remove-Item -LiteralPath (Join-Path $outPath "failed.txt")
}

$summary = @(
  "srvros libc++ probe",
  "libcxxRoot=$libcxxRoot",
  "archive=$archive",
  "requested=$($requestedSources -join ', ')",
  "compiled=$($compiled -join ', ')",
  "failedSources=$($failedSources -join ', ')",
  "failed=$failed"
)
$summary | Set-Content -LiteralPath (Join-Path $outPath "summary.txt")

if ($failed) {
  Write-Host "srvros libc++ probe reached the expected compile frontier. See $outPath"
  if ($FailOnError) {
    exit 1
  }
  exit 0
}

Write-Host "srvros libc++ probe completed. See $outPath"
exit 0
