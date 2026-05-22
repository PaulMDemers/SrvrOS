param(
  [string]$Sysroot = "build\sysroot\srvros",
  [string]$OutDir = "build\node-srvros-toolchain-probe"
)

$ErrorActionPreference = "Stop"

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
if (!(Test-Path -LiteralPath (Join-Path $sysrootPath "lib\crt0.o"))) {
  throw "Missing exported crt0.o in $sysrootPath"
}

New-Item -ItemType Directory -Force $outPath | Out-Null

$cSource = Join-Path $outPath "node_cross_probe.c"
$ccSource = Join-Path $outPath "node_cross_probe.cc"
$cObj = Join-Path $outPath "node_cross_probe.c.o"
$ccObj = Join-Path $outPath "node_cross_probe.cc.o"
$elf = Join-Path $outPath "node_cross_probe.elf"

@'
#include <stdio.h>
#include <unistd.h>

extern int node_cross_cpp_value(void);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("srvros node cross probe: pid=%d cpp=%d\n",
        (int)getpid(),
        node_cross_cpp_value());
    return 0;
}
'@ | Set-Content -LiteralPath $cSource -NoNewline

@'
extern "C" int node_cross_cpp_value(void) {
    return 42;
}
'@ | Set-Content -LiteralPath $ccSource -NoNewline

$common = @(
  "-target", "x86_64-freestanding-none",
  "-ffreestanding",
  "-fno-stack-protector",
  "-fno-stack-check",
  "-fno-lto",
  "-fno-PIC",
  "-ffunction-sections",
  "-fdata-sections",
  "-m64",
  "-march=x86_64",
  "-mabi=sysv",
  "-mno-80387",
  "-mno-mmx",
  "-mno-red-zone",
  "-I", (Join-Path $sysrootPath "include"),
  "-O2",
  "-g"
)

& $zig cc @common -std=gnu11 -c $cSource -o $cObj
& $zig c++ @common -std=gnu++20 -fno-exceptions -fno-rtti -c $ccSource -o $ccObj

& $zig ld.lld `
  -m elf_x86_64 `
  -nostdlib `
  -static `
  -z max-page-size=0x1000 `
  --gc-sections `
  -T (Join-Path $sysrootPath "lib\app.ld") `
  (Join-Path $sysrootPath "lib\crt0.o") `
  $cObj `
  $ccObj `
  (Join-Path $sysrootPath "lib\libsrvros.a") `
  -o $elf

$readelf = Get-Command readelf -ErrorAction SilentlyContinue
if ($readelf) {
  & $readelf.Source -h $elf
} else {
  Get-Item -LiteralPath $elf | Select-Object FullName, Length
}
Write-Host "srvros Node toolchain probe linked: $elf"
