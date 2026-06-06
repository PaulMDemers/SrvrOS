#!/bin/sh
set -eu

out_dir="${1:-a1466-macos-diag-$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$out_dir"

run() {
    name="$1"
    shift
    echo "collecting $name"
    "$@" >"$out_dir/$name" 2>&1 || true
}

run system-profiler.txt system_profiler SPHardwareDataType SPUSBDataType SPPCIDataType
run ioreg-ioservice.txt ioreg -p IOService -w0 -l
run ioreg-ioacpi.txt ioreg -p IOACPIPlane -w0 -l
run ioreg-devicetree.txt ioreg -p IODeviceTree -w0 -l
run hsspi-controller.txt ioreg -r -n AppleHSSPIController -w0 -l
run hsspi-device.txt ioreg -r -n AppleHSSPIDevice -w0 -l
run hsspi-keyboard.txt ioreg -r -n AppleEmbeddedKeyboard -w0 -l
run hsspi-trackpad.txt ioreg -r -n AppleMultitouchDevice -w0 -l
run lpss-spi.txt ioreg -r -n AppleIntelLpssSpiDevice -w0 -l
run pci-xhci.txt ioreg -r -n XHC1 -w0 -l
run kextstat.txt kextstat
run uname.txt uname -a

echo "wrote $out_dir"
