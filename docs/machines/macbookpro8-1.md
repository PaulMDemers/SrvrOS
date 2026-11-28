# MacBookPro8,1 bring-up record

Updated November 28, 2026. State **Native console output, EHCI enumeration
of the built-in topcase and workspace flash drive, and return to Ubuntu
verified. Both controllers completed their scans and released DMA in the
enumeration trial. The operator verified built-in keyboard input, command
execution and readable mixed-case text in the regular monitor. The isolated
trackpad trial verified Wellspring mode recovery, one-finger motion, two
contacts, and click press and release. Ubuntu verified the release-matched
USB report. The native desktop verified pointer movement, physical clicks,
Ubuntu return, and the USB report. The instrumented retry passed with zero
malformed frames. The earlier malformed frame remains unexplained. The
BCM4331 native trials verified channel 10 radio initialization, beacons,
association, WPA2-PSK/CCMP, DHCP, and same-boot disconnect and reconnect
with gateway traffic on `Demers2.4`. The native desktop then verified
Connect, Disconnect, and reconnect from Settings with both USB reports.**

## Identity and baseline

- Apple `MacBookPro8,1`, board `Mac-94245B3640C91C81`, with firmware
  `MBP81.88Z.0050.B00.1804101331`. Intel Core i5-2415M, two cores and four
  threads, with 9.6 GiB visible to Ubuntu.
- Intel Sandy Bridge integrated graphics `8086:0126` drives the internal
  1280 × 800 LVDS panel under Linux. Internal storage is a 119.2 GiB SATA disk
  on Intel AHCI `8086:1c03`.
- The 6-series USB controllers are EHCI `8086:1c2d` and `8086:1c26`, with
  UHCI functions `8086:1c2c` and `8086:1c27`. The full-speed Apple ANSI topcase
  `05ac:0245` sits on port 2 of internal high-speed hub `0424:2513` and
  advertises an 8-byte endpoint-zero packet size. Linux uses HID for the
  keyboard interfaces and `bcm5974` for the trackpad. The attached MBR/exFAT
  flash drive takes a high-speed path through the other EHCI controller.
- Intel HDA `8086:1c20` exposes a Cirrus Logic CS4206 analog codec
  `1013:4206` with subsystem `106b:1c00` and Intel HDMI audio. The wired and
  wireless controllers are Broadcom `14e4:16b4` and `14e4:4331` respectively.
- Ubuntu 26.10 boots via its UEFI entry and is the verified GRUB default.
  Secure Boot is unsupported on this host. The EFI partition is mounted and
  has space for staging. Ubuntu is the recovery OS for both installed releases.
- The privileged Linux inventory has 58 files, including 17 ACPI tables,
  with zero missing or partial reads. Raw evidence is kept locally. The host
  tool check, baseline build and QEMU xHCI UEFI USB smoke passed on `61111819`.

## Active scope

`device/macbookpro8-1/bringup` starts from `origin/main` at
`61111819eb4d99bc397b0cdb944bd34bfa9813da`. This branch owns the initial
MacBookPro8,1 console, input and USB workspace milestones. The EHCI split
trials established the topcase descriptor path and verified the workspace
flash drive with a 600 ms bound for its initial device descriptor. The active
input milestone covers the captured keyboard and trackpad through EHCI
periodic interrupts. The attended RAM-only profile verified Wellspring5 Type 2
reports after mode recovery. Its implementation scope is the
EHCI split control and periodic paths, topcase descriptor matching, boot
staging and their fixtures. It reuses the existing Apple trackpad decoder.
Pointer mapping and native workspace reads and writes follow as separate
milestones.

The scope check found separate Air7,2 Intel display work at `7f276f8c` in
PR #301, Pro11,2 USB Wi-Fi work at `b52a0974`, and Apple Silicon runtime, login and export work
in PRs #551, #550 and #510. The initial MacBookPro8,1 scope has no identified
dependency on those branches. The Air7,2 branch at `85aeb4ba` also touches the
staging tool for graphics receipts, independently of the EHCI profile.
Coordinate shared HID, storage or initialization changes if they become
necessary. [Device coordination](../agent-coordination.md)

The next trackpad block starts from `a3e1c806` on the same `61111819` base.
Ubuntu uses alternate setting 1 (multi-TT) on the captured `0424:2513`
root-connected hub and submits 478-byte interrupt requests to endpoint `0x81`
after the `08 05` to `01 05` feature-mode transition. This block matches that
hub setting and tests consecutive requests on a single periodic queue. Scope
is the explicit EHCI trackpad trial, its enumeration and DMA fixtures, and
the existing release-bound USB report. Keyboard, xHCI and ordinary startup
keep their existing paths. No overlapping EHCI work was found in PR #301,
the Pro11,2 USB Wi-Fi branch, or PRs #551, #550 and #510. The checkpoint is an
exact-stage VM rehearsal followed by an attended native one-shot and USB
report collection.

The mode timing trial starts from `69c6e085` on the same `61111819` base.
It prepares the trackpad queue and displays the checklist before switching
the device mode. It then times the first queue activation and completed
transfer and saves the first payload in the release-bound USB report. The
feature readback moves to the end of capture, before mode restoration.
The scope is the EHCI trackpad trial, its control completion timestamp,
and its fixtures. The current open PRs and Pro11,2 USB Wi-Fi branch have
no overlapping EHCI work. The native checkpoint follows the exact-stage
VM rehearsal and uses the same Ubuntu recovery and prepared USB workspace.

The raw endpoint trace starts from `936d6c77` on integration base `61111819`.
The current `origin/main` at `c523e036` and open device scopes have no
overlapping EHCI changes. Keep the HID replay and endpoint `0x81` queue from
the last trial, record a length distribution and bounded raw samples, then
poll the sibling endpoint `0x84` separately in the same feature mode.
Compare with Ubuntu USB completions from this unit. Scope is the attended
EHCI trackpad trial, its queue fixture and existing USB report. The next
checkpoint is exact-stage VM rehearsal and an attended one-shot with Ubuntu
recovery.

The Wellspring recovery trial starts from `029910c2` on base `61111819`.
`origin/main` at `c523e036` and the open device scopes have no overlapping
EHCI changes. If an eight-byte response arrives on `0x81` before a valid
frame, the attended profile will pause at a verified retired request, select normal
mode, wait at least 1 ms, and retry Wellspring mode once. It will preserve
the queue head and data toggle, record the before and after results, and
save the report to the prepared flash drive. Scope is the existing EHCI
trackpad trial and its fixtures, with an exact-stage VM rehearsal before
the native one-shot. [Linux mode recovery](https://github.com/torvalds/linux/commit/fc1e8a6f129d87c64ac8e58b50d9dfa66217cfda)

The desktop trackpad block starts at `ea08f5c9` on base `61111819`.
`origin/main` at `93e10a44` and the open device scopes have no overlapping
EHCI input work. The captured topcase will feed the existing desktop mouse
queue from a resident EHCI worker and reuse the verified Wellspring recovery.
The attended boot will open a pointer and physical click checklist in Text
in the regular compositor. After the desktop exits, it will retire the topcase
controller and save the release-bound report through the separate flash
controller. Scope is EHCI trackpad ownership, the boot profile and desktop
checklist. The checkpoint is exact-stage VM rehearsal and native acceptance
on this unit.

The desktop USB follow-up starts from `7ed554ef`. The first desktop run
confirmed working trackpad input with the operator and returned to Ubuntu,
but the flash save failed. A read-only replay using the exact initramfs and
attached drive reproduced the writable-admission refusal. The follow-up
consumes `origin/main` at `93e10a44` and the boot-inode repair in PR #554.
Scope is integrating the landed fix, replaying this unit's USB workspace, and
verifying the same desktop checklist with a native flash report.

The malformed-frame trace starts from `628deda5` on `93e10a44`. The
`37de62ed` integration baseline and open device scopes have no overlapping
EHCI input changes. The resident `0x81` worker will retain a bounded set of
rejected frame bytes and the observation timing before republishing the
transfer descriptor. Scope is diagnostic capture and its fixture. The
attended retry will use the same desktop checklist and release-bound USB
report.

The combined desktop input block starts at `ecc9e0c4` on `93e10a44`.
`origin/main` at `37de62ed` and open PRs #551, #550, #510 and #301
have no overlapping EHCI input work. The captured keyboard and trackpad
share one transaction translator and will run under one EHCI worker. Scope
covers the periodic queues, the desktop checklist and its VM rehearsal.
Native acceptance requires built-in typing, pointer motion and clicking in
the compositor, safe retirement and a release-matched flash report.

The combined-input recovery block starts at `90f0d3b3` on `93e10a44`.
`origin/main` at `54632d47` and open device scopes have no overlapping
EHCI work. Scope is the captured topcase queue instrumentation, an early
report on the separate flash controller, and the attended desktop exit
path. The next trial will bound the observations, count keyboard reports
without retaining keystrokes, and keep an independent pre-desktop report
if input failure prevents the final report.

The three-boot repeatability check starts at `a1f21d9a` on `93e10a44`.
`origin/main` at `ae1a64d8` and the open device scopes have no overlapping
EHCI work. It reuses the accepted `3388f68afd5dc712` payload and checklist.
Only the staging identity changes so each boot can write two new files to
the same workspace. Collect and verify each run before arming the next.

The persistent Wellspring recovery block starts at `5086e27a` on
`93e10a44`. The integration tip `917e7c5c` and the open device scopes have
no overlapping EHCI work. Scope is the model-gated desktop trackpad's
startup retry policy, transport fixture and native reports. The retry
uses the observed eight-byte `0x02` response, keeps the captured shared
frame list and keyboard queue, and records the result of each attempt.
The checkpoint is a native desktop run with both reports verified.

Qualification of the revised recovery begins at `4342f2a5` on `93e10a44`.
The integration tip `d29abcf1` and open device scopes have no overlapping
EHCI work. Reuse the `be59d55498834452` payload across three attended warm
boots. Bind each startup and final report to its own release and collect
each pair before arming the next boot. Reassess trackpad initialization if
the persistent eight-byte response stream recurs.

The desktop saved-file block starts at `c6fbdbc4` on `93e10a44`.
The integration tip `e1b4ae47` and the open device scopes have no
overlapping EHCI storage work. Scope is a bounded desktop trial using
the captured flash controller and the selected exFAT workspace. Text
will edit and reopen one release-bound document while the keyboard and
trackpad run. The checkpoint includes startup and final USB reports,
safe controller retirement and Ubuntu readback of the edited file.

The display-brightness block starts at `c324781e`, with merge base
`93e10a44` and integration tip `5cb57eab` inspected. It reuses the local
desktop and flash-report path from `75525ae902f943ab`. Scope is the captured
Sandy Bridge LVDS backlight provider, its host fixture and one attended
Settings trial. PR #301 covers guarded Intel GPU ownership and GT reset.
This block leaves GPU ownership and modesetting with firmware and has no
dependency on that PR. The checkpoint is a visible dim and brighten in
Settings, hardware readback, and both release-matched USB reports.

Ubuntu exposes `acpi_video0` for the internal LVDS panel. On this unit,
switching Ubuntu's brightness from index 15 to 12, 7 and 14 moved the
Intel CPU PWM duty at `0x48254` from `0x0710` to `0x05a6`,
`0x034c` and `0x0690`. Restoring index 15 restored `0x0710`.
The PCH period stayed at 1,808 and both PWM enable words stayed
`0x80000000`. Source `2bbb3e0b` adds the trial-only Settings provider
with the exact GPU, PCH, DSDT and PWM-state checks. Release
`4befbb5325d5ed2f` passed both exact VM paths, with the provider
unavailable in Q35 and both USB reports verified. Its installed recovery
is `c1a8e397a41363dc`.

The November 27 native trial passed. Settings initially read 94%. The
operator saw the panel dim and brighten, and the register readbacks
matched all 68 requests across 30–76%. Exiting restored the original
duty of 1,707 out of 1,808. The built-in keyboard matched the Text
checklist and the trackpad recorded 4,678 reports with zero malformed
frames. Ubuntu verified both reports directly from the EHCI flash
workspace after return. The read-only allocation check passed with 145
files and 153 clusters, and all pre-existing user files were preserved.
The one-shot entry was consumed, and the exact payload and reports are
archived outside `build`. Brightness is enabled by the explicit
`--ehci-desktop-brightness` recipe on this unit.

The built-in speaker block starts at `29fe70a9` on merge base `93e10a44`,
with integration tip `dc4a48d3` and the open device scopes inspected. It
adds an opt-in Intel PCH HDA and Cirrus CS4206 speaker profile, a codec
fixture and an attended Settings trial using the accepted live EHCI
workspace. It uses the built-in speaker and bass routes from this unit's
Ubuntu capture. The checkpoint is audible left and right playback,
verified stream stop, and both release-matched USB reports after Ubuntu
return.

Source `53ce0ca9` and release `f88d28de4f58200d` verified the enabled
CS4206 routes, five completed test streams and muted stop. The operator
heard two of the intended three cues at each setting, so audible stereo
coverage remains incomplete. Ubuntu verified both startup and final
reports and a clean allocation check with 147 files and 155 clusters.
An Ubuntu PipeWire comparison with longer gaps and three pitches also
produced two audible cues. PipeWire showed the same gain for both
channels, with the analog sink mapped to front left and front right.

Source `109b1f19` and release `024a87e1e8d4bf42` used the longer gaps and
three pitches. The operator identified a broken physical left speaker
on this laptop. Right-speaker output was audible. SrvrOS completed all
three playback attempts without controller errors, stopped the stream
and read back the disabled amplifier GPIOs. Audible left-channel
coverage remains open. The desktop checklist timed out, and the
automatic exit saved both reports and returned to Ubuntu. Ubuntu
verified both files, retained all pre-existing user files and verified
149 files and 157 clusters in the read-only allocation check. The
accepted brightness profile and known-good recovery payload were
retained alongside the speaker trials.

The fan and CPU temperature block starts at `ac1971f4` on merge base
`93e10a44`, with integration tip `dc4a48d3` inspected. Open PRs #567 and
#301 cover graphics, with no telemetry dependency. Scope is the observed
Sandy Bridge thermal signature, one SMC exhaust fan, and an automatic
System Sensors trial with startup and final reports on the EHCI workspace.
The checkpoint is fresh boot-CPU and package temperatures, an exhaust fan
RPM reading, increasing sample counts, and normal return to Ubuntu.

Ubuntu exposed one Exhaust fan with a firmware range of 2,000–6,200 RPM.
The live DSDT matched the captured table byte for byte. Its APP0001 SMC
declares I/O `0x300–0x31f` and IRQ6. SrvrOS uses the data and command
ports at `0x300` and `0x304` for metadata and reads of `FNum` and
`F0Ac`. The fan remains under firmware control. The i5-2415M reported
CPUID signature `0x206a7`, core and package thermal support, and a 100 °C
reference from MSR `0x1a2`.

SrvrOS `70118226` with SrvrApps `ab69301` passed the native telemetry
trial in release `5e4d9efffb95d338`. System opened directly on Sensors
and reported both providers fresh. CPU core and package read 77 °C at
startup and 65 °C at exit. The fan read 3,100 and 2,182.75 RPM at those
checkpoints. Both sample counts advanced from 1 to 27 during the
25-second desktop trial. SMC completed 30 transactions with zero
failures and reached a transaction boundary before shutdown. Ubuntu
returned automatically, verified both flash reports and preserved all
existing user files. The read-only allocation check passed with 151
files and 159 clusters. The exact payload and reports are archived
outside `build`. This verifies the boot CPU and package readings on
this unit through the explicit `--ehci-desktop-telemetry` profile.

The battery and charger block starts at `c0881211` on merge base
`93e10a44`, with integration tip `dc4a48d3` inspected. Open graphics
work is independent. Scope is the exact unit's read-only SMC battery and
adapter keys, the existing cached battery interface and an attended
System Sensors trial. The checkpoint is fresh connected, disconnected
and reconnected readings, signed current, and both EHCI flash reports.
The SMC path requires no EC/SCI or AML ownership.

Ubuntu's charger cycle correlated SMC `ACIN` with ADP1 and the signed
`B0AC` current with BAT0 discharge. The sampler also verifies `BBIN`
battery presence and reads `B0AV`, `B0RM`, `B0FC` and `BRSC` for
voltage, remaining and full charge, and gauge percentage. Matching ACIN
reads bracket each sample. The SMC gauge was 98% while Ubuntu's BAT0
reported 92% during the comparison. Those values retain their respective
provider identities.

Source `f0651539` and native release `0cb82183e1678a63` passed the
three-phase System Sensors trial. Each phase used three consecutive
fresh samples. The accepted checkpoints recorded 12.433 V and 0 A on
AC, 12.234 V and −1.382 A unplugged, then 12.418 V and 0 A after
reconnect. The SMC gauge read 98% at each checkpoint. Twenty samples
completed with zero rejections, and SMC reached a transaction boundary
before shutdown. Ubuntu returned automatically and verified both reports.
The read-only allocation check passed with 153 files and 161 clusters,
and all existing user files were preserved. The release and reports are
archived outside `build`. This unit uses the explicit
`--ehci-desktop-battery` recipe for the read-only trial.

The integration check starts at `c28c8fd0` and brings in shared baseline
`dc4a48d3`. The conflicts overlap the Air6,1 storage work in the kernel
boot dispatch, hardware staging, and the exact-stage VM harness. Keep
both machines' paths and the published SrvrApps Sensors revision. The
checkpoint is one attended native desktop boot with built-in keyboard
and trackpad input, the Text save and reopen check, both USB reports,
and Ubuntu return before merging the bring-up PR.

The integrated source `513def33` booted release `7cf2eafd5fb1972a` on
this MacBook. The 1280 × 800 desktop checklist passed both pointer axes,
maximize and restore, and pointer-driven exit. EHCI recorded 1,025 valid
trackpad reports, five click presses and releases, and 28 valid keyboard
reports. Text saved and reopened the 550-byte document. Its original
542 bytes were preserved and the edit contained `Ehci-42` followed by
a newline. The trial's exact-text check requires only the seven-character
suffix and reported `INCOMPLETE` for that check. The trackpad mode was
restored and both EHCI queues retired safely.

Ubuntu consumed the one-shot entry and verified the startup and final
reports, the recovery payloads, and all previous user files. The read-only
allocation check passed with 156 files and 164 clusters. The exact payload
and both reports are archived outside `build`. The exact-stage VM passed
the desktop and keyboard-only exit paths. The integrated branch also
passed the EHCI hub and xHCI console smokes and the Air6,1 SSD probe's
foreign-platform refusal.

The wired Ethernet block starts from merged baseline `24feebdd`. The
open Air6,1 and Air7,2 graphics PRs and the Apple Silicon runtime PRs
have no Ethernet dependency. This checkout owns the exact BCM57765
`14e4:16b4` revision `10` at `02:00.0`, its standalone RAM boot
recipe, and the EHCI USB reports. Ubuntu's `tg3` reports firmware
`57765-v1.37`, PHY address 1 with ID `0362:5e44`, and a negotiated
1 Gb/s full-duplex link. BAR0 and BAR2 are each 64 KiB.

The first native checkpoint is PCI and ASIC identity, bounded PHY
register reads, carrier and link mode, verified MMIO retirement, and
two release-matched reports after Ubuntu returns. It precedes reset
ownership, descriptor and DMA setup, and packet exchange. The standard
network provider selection stays outside this probe.

Release `d6053aaf8786343a` enumerated the controller natively and
saved both reports. The first ASIC gate compared a truncated value
against the full register. The native probe refused before MMIO and
returned to Ubuntu. The privileged Ubuntu PCI read confirms raw ASIC
revision `0x57785100`. The retry uses that exact value.

Source `a23334fc` and release `c12c6db3391e7bc4` passed the native
link checkpoint. PCI memory decoding and bus mastering were already
enabled in D0. The probe used the existing decode, issued read commands
to PHY address 1, and observed ID `0362:5e44`, BMSR `0x796d`, and
auxiliary status `0x871c`. These decoded to carrier at 1,000 Mb/s
full duplex and matched Ubuntu. The temporary BAR mapping retired with
MDIO idle and the captured PCI state intact. Both startup and final
reports verified after the automatic Ubuntu return. The read-only
allocation check passed with 160 files and 168 clusters. The exact
payload and both native attempts are archived outside `build`.

The link checkpoint used no packet rings or DMA and registered no
network provider. The next acceptance step is a BCM57765 packet test
with bounded DMA ownership and retirement, followed by the normal
network stack's DHCP and local gateway checks. The Linux
[tg3 register definitions](https://github.com/torvalds/linux/blob/master/drivers/net/ethernet/broadcom/tg3.h)
provide the PHY status reference.

The packet block continues PR #568 from device source `6606f6e7` and
integration base `24feebdd`. This unit owns the BCM57765 polling path,
its early-boot recipe and EHCI reports. The open graphics, desktop login,
shared memory and installation export PRs have no Ethernet overlap.
The trial will verify an exact frame through the internal MAC loopback,
switch to the physical link for DHCP through the existing IPv4 stack,
then retire the provider and DMA before returning to Ubuntu. It uses a
single standard receive ring, receive return ring and send ring. The
startup report is saved before packet DMA is admitted.

Native releases `94abe4b864708a88` and `68a8a7c71f096dcc` refused
before enabling packet DMA. The second run isolated the refusal to the
firmware's SRAM window selector, which was left at `0x0c04` with the
packet engines idle. Both reports from each run verified after Ubuntu
returned. The retry accepts that exact selector and restores it after
each indirect SRAM access.

Source `3db072e0`, release `4236ecafdcb7e339`, passed the native
packet and DHCP trial. The MAC loopback returned the exact 60-byte frame
with one TX and one RX completion. After loopback was disabled, the
existing stack completed one Discover, Offer, Request and ACK on the
wired link and configured IPv4 and a default router. DHCP took 1,846 µs
in the bounded poll. Across both phases, all three TX and three RX
descriptors completed, with zero drops and zero reported read/write
DMA errors. The trial retired the provider, stopped bus mastering and
released the DMA allocation. The exact-stage VM confirmed refusal on
foreign hardware, both USB reports and one-shot return.

Ubuntu verified both native reports and the retained recovery payloads.
The read-only exFAT check passed with 166 files and 174 clusters. Earlier
workspace files were preserved. All trial payloads and reports are
archived outside `build`. This is an opt-in polled boot trial. Ordinary
desktop networking and link loss/reconnect remain separate acceptance
work.

The desktop cable block continues PR #568 from `35c8456e` on
`24feebdd`. It extends the BCM57765 lifetime across the existing EHCI
desktop and adds an attended checklist to the compositor. The open
graphics, login, shared-memory and export PRs do not touch this provider
or the cable state machine. The checkpoint is connected DHCP and
gateway traffic, observed cable loss and address retirement, renewed
DHCP and gateway traffic after reconnect, and both USB reports with
clean topcase, network and flash retirement.

Release `7dd94caf8a4f6945` passed connected DHCP and gateway echo on
the desktop. Unplugging cleared carrier, IPv4 and the resident worker.
Reconnect restored 1 Gb/s carrier and completed the Discover transmit.
The second DHCP round exhausted its poll count after 48,395 µs before
the time-based retry. Text startup also cleared the first cable prompt
after the compositor had logged it. Both native reports were verified,
and DMA retirement and Ubuntu return succeeded. The next candidate
keeps the prompt visible across app launches and paces DHCP polling so
the existing elapsed-time retry can run.

Source `4a60c26d`, release `fd24bb312a64131f`, passed the attended
desktop cable cycle. Text opened the checklist and the compositor
recorded all three phases. The initial connection and reconnect each
accepted a DHCP Offer and ACK, received a gateway echo and started
the same resident polling worker. Unplugging cleared IPv4 and the
default router and parked the worker. The paced DHCP rounds took
15,311 µs and 19,492 µs and needed one Discover each. The host fixture
separately proved the elapsed-time retry when the first Discover
was lost. The VM exercised the checklist, foreign-platform refusal,
both reports and one-shot return.

The native controller completed ten TX and fourteen RX descriptors
with zero drops, then stopped bus mastering and released DMA. Topcase
and flash retirement succeeded. Ubuntu verified both reports and all
previous workspace files. The offline exFAT check passed with 170
files and 178 clusters. The payload and reports are archived outside
`build`. The desktop cable recipe remains explicit. Ordinary boot
and sustained multi-cycle networking have separate acceptance.

The next work block enables the exact MacBookPro8,1 BCM57765 on ordinary
desktop startup from integration baseline `24feebdd`. The local branch
owns the provider, wired carrier and DHCP lifecycle, and release-specific
desktop trials in draft PR #568. The open graphics, login, shared memory
and export work has no Ethernet dependency. First qualify the staged
desktop in the VM, then boot with carrier present and absent and measure
gateway traffic through three cable cycles. Verify both flash reports
and Ubuntu recovery after each boot before recording acceptance.

Source `8d5af090`, connected-start release `657931e51d2152c8`, booted
the production wired provider with carrier present and acquired DHCP.
The desktop measured 35 gateway replies from 35 requests over 18.7
seconds. The controller completed 51 TX and 51 RX descriptors with zero
drops, then released DMA and stopped bus mastering. Ubuntu verified both
flash reports and preserved the existing workspace files. The offline
exFAT inspection passed with 172 files and 180 clusters. The same source
passed disconnected startup and three cable cycles on release
`e0c0bff8e7ef17ae`. The desktop opened without carrier or an IPv4 lease.
The first cable connection and all three reconnects negotiated 1 Gb/s
full duplex and acquired DHCP. Each unplug cleared IPv4 and the default
route while leaving the resident receive worker running. Each connected
phase returned all 35 gateway echoes over 17.4 seconds, for 140 of 140
across four phases. The controller completed 230 TX and 312 RX descriptors
with zero drops and verified DMA retirement. Ubuntu verified the hashes
of both reports and preserved the existing workspace files. The offline
exFAT inspection passed with 174 files and 182 clusters.

### Internal BCM4331 Wi-Fi

This block uses integration baseline `24feebdd`, the working Ubuntu `b43`
reference, and the verified EHCI report path. The captured PCIe function is
`03:00.0`, `14e4:4331` revision `02`. Linux reports chip package `09`, BCMA
ChipCommon revision `25`, D11 revision `1d`, and PCIe core revision `13` in
hex. Its HT PHY is type 7, revision 1. Firmware `784.2` joined the 2.4 GHz
test network on channel 10 and returned all four interface-bound gateway
probes while Ethernet stayed up.

The first SrvrOS checkpoint owns the exact MacBookPro8,1 PCIe and BCMA
probe, isolated boot recipe, and two release-specific flash reports. It
reads the chip and core topology and verifies restoration of temporary PCI
access before returning to Ubuntu. Firmware and board data, passive scan,
authentication, and DHCP on the wireless interface are subsequent native
checkpoints. The MediaTek work from PR #19 supplies Wi-Fi service and IP
stack references. The active Pro11,2 branch at `bf7a0112` covers external
USB Realtek hardware. It does not share the BCM4331 PCIe transport.

The first native release `867b4df92cd39cdc` read the expected ChipCommon
ID `13924331`, capabilities `58500000` and EROM pointer `18107000`.
Ubuntu left PCI bus mastering enabled across the warm reboot, so the
probe skipped the movable BAR window. Both reports and Ubuntu return
verified. The retry clears BME and checks PCIe transaction drain before
switching the BAR window. It restores the original selector and memory
decode, with DMA fenced through the return to Ubuntu.

The retry `dbe360115542ce8e` fenced DMA and read the EROM. Its parser
stopped at the PCIe core's 64-bit bridge aperture. Release
`a562b8bab2297e39` consumes those descriptors while admitting only the
captured 4 KiB core windows. It verified ChipCommon, D11 and PCIe at
`18000000`, `18001000` and `18002000` and the D11 wrapper at `18101000`.
The wrapper reported the inherited D11 clock enabled and reset clear.
All six reports from these three boots saved and Ubuntu verified each
return. The board-data follow-up samples the clocked PHY and temporarily
selects the board ROM pins with verified restoration before firmware work.

Release `81792b9f74d35454` identified HT PHY type 7 revision 1 and analog
revision 9, matching Ubuntu. It read revision 9 of the 220-word board ROM
with a valid CRC after selecting its pins, then restored the selector,
pin mux and mappings. Both flash reports verified and both Ubuntu links
came back. The next isolated recipe pins Ubuntu's D11 microcode bytes and
checks for a fresh `784.2` firmware response with the MAC disabled and DMA
fenced. PHY initialization and the channel 10 network trial follow.

Release `57f79380ace8e3fd` wrote all 10,804 microcode words. The warm
D11 state immediately reasserted its DMA interrupt after the interrupt
clear, so the probe stopped before it started the PSM. Both flash reports
and Ubuntu return verified. The retry uses a bounded D11 reset to clear
the inherited device state and verifies the HT and PLL clocks before
loading the same microcode.

Release `6a13ab07775b6428` passed the D11 reset and clock checks and
bootstrapped the complete pinned microcode. The PSM raised a fresh MAC
suspend event and returned firmware revision `784.2`. It halted with the
MAC disabled and PCI bus mastering fenced. Both USB reports verified,
the workspace allocation check passed with 186 files, and Ubuntu resumed
with both links up. The next native checkpoint is HT PHY and radio
initialization and passive receive on channel 10. Association, WPA2 and
interface-bound DHCP and gateway traffic remain to be verified in SrvrOS.

The channel 10 block starts from `fc1cb4f2` and fetched integration
baseline `24feebdd`. It owns the BCM4331 HT PHY and radio initialization,
passive receive, beacon matching, and release-specific EHCI receipts.
Use Ubuntu's working channel 10 session to measure the radio sequence
and check timing and receive format against the Linux driver. The first
native acceptance is a bounded receive window with a verified target
beacon and safe retirement. The Pro11,2 USB work at `4226e6f2` and the
open graphics and runtime PRs have separate hardware and paths.

Ubuntu's driver trace recorded the radio power-up, HT table programming
and channel 10 switch. The passive profile keeps the receive setup through
the last HT table and omits the driver's later TX sample calibration.
The associated AP advertises `Demers2.4` at 2457 MHz. The SrvrOS trial
checks the pinned local HT profile and firmware tables, requires fresh
radio calibration status, and validates complete beacon frames.

Release `bfa0d0c0f1f159de` completed the pinned init writes, radio
calibration, HT programming and channel 10 sequence. The receive gate
expected the entire DMA control register to be zero and never started
PIO. Firmware retirement, both flash reports and Ubuntu return verified.
The retry measures the initial control and status, checks disabled DMA
separately, and changes only the direct-FIFO control bits.

Release `c062a4edf12fc2ab` verified the control register's reset default
`0x40`, inactive RX DMA and PIO admission. It delivered one frame with
channel word `0x57`, which identifies channel 10 and HT PHY. It did not
match the target beacon. Both reports and safe return verified. Release
`f3e4d76b26c3d17f` acknowledged 31 receive events and read one frame whose
data repeated the channel word at the expected frame control offset.
Both reports and safe retirement verified again.

An Ubuntu monitor comparison on 2457 MHz reported zero target beacons
with forced PIO and 58 with DMA over identical eight-second captures.
The first PIO comparison had counted tcpdump's empty shutdown line as
a frame. Ubuntu's default DMA driver and both connections were restored.
The next trial uses a bounded DMA64 receive ring on the verified D11
engine. It checks all DMA channels are idle before enabling bus mastering,
posts a finite batch of receive buffers, and verifies the engine, PCI
fence and ring are retired before releasing memory. The exact-profile
trial leaves transmit queues disabled.

Release `7a49dd4251a58927` initialized the radio and saved both reports.
The DMA preflight stopped at channel 1 before granting bus mastering.
Readback with Ubuntu confirmed the BCM4331 has six TX register banks
and only one RX bank. RX control and status read all ones in channels
1 through 5. The revised preflight requires that exact topology while
checking the real RX and all six TX engines are disabled.

Release `116b5ca72139453e` initialized the radio and armed the RX ring.
The hardware pointer stayed at zero and no beacons were captured. The
MAC suspended and PCI bus mastering was fenced, but the RX engine did
not confirm disable. The trial withheld its DMA pages and saved both
reports before Ubuntu returned. The next retry verifies the ring base
before granting bus mastering, enables RX before posting descriptors,
and can hold D11 in reset after fencing PCI if RX retirement stalls.

Release `84541853f67eb293` verified normal RX shutdown, PCI fencing, and
ring clearing with zero DMA pages retained. The active descriptor moved
to `0x10`, but no frame completed. Both flash reports, workspace integrity
and Ubuntu return verified. The next retry supplies the Linux driver's
post-PHY MAC timing, capability and PHY information to the firmware.

Release `77845b0ad46fd3cb` (`6c7ef299`) passed the exact-stage VM check
and returned `BEACON_OK` on the MacBook. D11 firmware `784.2` reached
active state after HT calibration and channel 10 tuning. Five DMA frames
completed with no timeout. Three were checksum-valid `Demers2.4` beacons
with channel 10 in the hardware receive header and beacon information.
The MAC suspended, the RX ring stopped and cleared, PCI bus mastering
was fenced, and all DMA pages were released. Both flash reports verified.
The workspace allocation check passed with 200 files and zero leaks or
mismatches. Ubuntu returned with Ethernet and Wi-Fi connected. The exact
payload and reports are archived in the local recovery directory under
`77845b0ad46fd3cb`. Association and wireless IP remain subsequent tests.

The station trial starts from `f50c74bf` with fetched integration baseline
`24feebdd`. It owns BCM4331 D11 transmit and station lifecycle, a bounded
Demers2.4 connection recipe, and the two existing EHCI receipts. The
first checkpoint is Open System authentication and association with
verified wireless transmit status. Then use the existing WPA2 handshake
and software CCMP code, and qualify an interface-bound DHCP exchange.
The USB MediaTek driver from merged PR #19 and the Pro11,2 USB trials
are independent transports. PR #568 tracks this device branch. The open
graphics and runtime PRs have no overlapping D11 work. Keep the accepted
passive release and Ubuntu recovery available throughout.

Release `cbd2537e8ca4d612` (`691a58c9`) passed the exact VM rehearsal and
native station test. The AP acknowledged the Open System authentication
and association frames and returned status 0 for both. The station sent
an acknowledged disassociation. RX and TX stopped and PCI bus mastering
was fenced. Both flash reports verified, and Ubuntu returned with both
links up. The station radio profile includes the TX gain calibration from
this unit's Linux trace. Ubuntu's sanitized b43 capture confirmed the
1 Mb/s management header and the D11 transmit queue.

Release `5ae54bef0deab874` from `00355005` passed the exact-stage VM
rehearsal and the native association, WPA2 and DHCP trial. The AP
acknowledged authentication and association. The four-way handshake
verified M3 and the GTK, and the AP acknowledged M4. The bounded IPv4
probe selected only `bcm4331` and observed one Discover, Offer, Request
and ACK over CCMP. Linux supplied fresh per-trial entropy and the PMK
from the active profile. The receipt verified software key retirement,
acknowledged disassociation, stopped RX and TX, fenced PCI bus mastering,
and zero retained DMA pages. Both flash reports and the offline allocation
check passed with 204 files. Ubuntu returned with Ethernet and Wi-Fi up.
The collector archived the reports and manifest, removed the temporary
credential payload, and restored the credential-free association entry
alongside the known-good recovery.

The gateway and reconnect trial starts from `76d2ee02` with integration
baseline `24feebdd`. It extends the BCM4331 station trial and its existing
flash receipts. It uses the shared bounded network connection scopes to
verify gateway echo replies before and after an acknowledged disconnect,
fresh association, WPA2 handshake and DHCP exchange in the same boot.
The other Pro's external USB transport remains independent. PR #568
carries the device branch. There is no overlap with the current graphics
and runtime work. Keep the accepted passive and association payloads and
Ubuntu recovery available.

Release `c0aab7606708081f` (`b75e7929`) passed the exact-stage VM
rehearsal and returned `RECONNECT_OK` on the MacBook. The first
connection completed WPA2 and DHCP and received five of five matched
gateway echoes. The disconnect was acknowledged, cleared IPv4 and the
default route, and retired the software keys. After a 500 ms interval
with the network link down, the station rejoined the same AP. It verified
a distinct authenticator nonce, completed a fresh DHCP exchange, and
received five of five gateway replies again. The final disconnect and
provider retirement completed, all 26 transmissions had acknowledgments,
and both DMA queues stopped with no retained pages. Both flash reports
verified and the offline allocation check passed with 206 files and zero
leaks. Ubuntu returned with both links up. The collector restored the
credential-free association entry and removed the temporary payload.

The desktop Wi-Fi work starts from `1a24c9c1` with integration baseline
`24feebdd`. The BCM4331 will publish the validated `Demers2.4` network
through the existing Settings service and use the normal network worker
after DHCP. Scope covers the BCM4331 desktop owner, fixed-channel Wi-Fi
publication, and the EHCI desktop checklist and reports. The existing
network stack selects a single adapter per boot. Use a provisioned Wi-Fi
desktop boot for this trial while retaining the wired desktop profile and
Ubuntu recovery. PR #568 carries the device branch. The Pro11,2 USB Wi-Fi
owner and the graphics and runtime PRs have separate hardware and scope.

Desktop release `d6defd5b7a97090b` uses SrvrOS `24738e27` and SrvrApps
`6818e00`. The exact-release VM verified navigation from Settings to Wi-Fi,
report saving, platform refusal, and the Ubuntu boot fixture. The MacBook
returned `DESKTOP_OK`. Settings issued Connect, Disconnect, and Connect
again to `Demers2.4` on channel 10. Both connections completed WPA2 and fresh
DHCP, then received 14 of 14 gateway replies. Disconnect cleared the lease,
route, and keys. The TX ring wrapped with 45 acknowledged transmissions,
and the driver released the RX and TX DMA allocations. Both flash reports
matched their recorded hashes. The offline exFAT check found 208 files,
216 allocated clusters, and no leaks or bitmap mismatches. Ubuntu returned
with its default intact, and the collector restored the credential-free
entry and removed the temporary private payload. The native run record is
`build/review/macbookpro8-1-ehci-trial-d6defd5b7a97090b/run.json`.

The signal-loss qualification starts from device commit `4f044783` with
integration baseline `24feebdd`. PR #568 carries the work. The scope is
passive observation of the associated BCM4331 access point and a bounded
gateway check when the signal returns. The instrumented desktop trial
retains the behavior of the previously accepted connection owner so it
can establish whether recovery already works. The trial depends on an
attended interruption of `Demers2.4`, the existing private station profile
and matching EHCI workspace. Next checkpoint is the exact-stage VM boot,
followed by the native signal-loss and restoration capture. The Pro11,2
USB Wi-Fi transport has a separate acceptance record.

Signal-loss candidate `6c3a127638e660eb` (`ef27c918`) passed the
exact-stage VM and brought up the native BCM4331 and both input queues.
Text opened the checklist. The desktop reached its 180-second deadline
with no recorded pointer or keyboard activity and no Connect request.
The run is incomplete before the outage step. Both flash reports were
verified, the offline allocation check passed with 210 files and no leaks,
and Ubuntu return and private-payload cleanup were confirmed.

The retry `575c07efdf1118a4` (`a13b854d`) recorded the Settings
connection, WPA2, DHCP, baseline beacon tracking and 14 gateway replies.
It then recorded a manual disconnect and reconnect from System, and all
14 gateway echoes succeeded again in the new epoch. No beacon outage was
measured during the original connection, so RF recovery remains open.
Both flash reports and Ubuntu return were verified, and the exFAT check
passed with 212 files and no leaks. The checklist now identifies the
router or access point as the control for the outage.

The local receive-gap trial starts from `551901ee` against integration
baseline `24feebdd` on PR #568. The owner cannot interrupt the access point.
The replacement trial keeps the normal desktop, station and gateway in
use, and temporarily drops received frames at the BCM4331 driver boundary.
It measures the original connection before, during and after that gap.
Scope covers the BCM4331 probe, the automatic desktop trial and release
reports. It depends on the existing station profile and EHCI workspace.
The previously staged attended router trial has not been armed.

The unused router candidate `08447922b7838c8b` was superseded without
arming a native boot, and its temporary private payload was retired.
The first automatic local trial `634b61584f369b05` (`83c2fc03`) associated
and completed WPA2. DHCP failed before the injection began, so the
trial returned incomplete. Both flash reports, 214 intact files, and
Ubuntu recovery verified. The retry delays automatic Connect until
after desktop startup and records DHCP Discover, Offer, Request, and ACK
counts in the final report.

The delayed retry `8cf559613c3b0b22` (`e982b06b`) completed WPA2
and recorded one Discover, one accepted Offer, one queued Request and
zero accepted ACKs before the two-second DHCP bound. Both reports
verified and Ubuntu acquired its lease after reboot. The next bounded
candidate adds one Request retransmission for the local BCM4331 trial
within the existing paced DHCP scope and deadline. Shared DHCP parsing,
lease checks and the policies of other callers are preserved.

Release `c809111f83871175` (`f4725bbe`) passed the automatic local
receive-gap test. The MacBook completed WPA2 and DHCP on the first
Request and received 14 of 14 baseline gateway replies. The driver
dropped 278 received frames over 15.009 seconds. During the gap the AP
acknowledged the radio transmission of a gateway probe, and SrvrOS
received no IP reply. Reception then resumed and all five gateway
probes replied on the original association and lease. All 29 radio
transmissions were acknowledged, and the provider and DMA retired.
Both flash reports matched their checksums. The offline allocation check
passed with 218 files and no leaks. Ubuntu returned and the collector
removed the temporary private payload. The access point stayed online,
so reacquisition after an access-point shutdown remains open.

The attended out-of-range trial starts from `94360dfd` and integration
baseline `24feebdd` on PR #568. The MacBook will leave and return to the
coverage of the associated Demers2.4 AP with the same flash workspace.
The BCM4331 trial records a measured beacon gap, post-return gateway
traffic and the connection epoch. The existing station recovery behavior
is the subject of the test. Scope covers the trial boot recipe, status
and reports. The Pro11,2 USB provider has separate hardware and scope.

The out-of-range boot `6a40ad67a02ce1a9` (`c090766c`) returned
`INCOMPLETE`. The BCM4331 refused PCI identity or mapping context before
Connect was queued, and the desktop exited at phase zero. No RF gap or
reassociation was observed. The operator reported that walking out of
coverage was impractical. Both flash reports matched their checksums.
The offline allocation check passed with 220 files and the prior files
intact. Ubuntu returned and the collector retired the private payload.
Access-point disappearance and automatic reassociation testing are
deferred at the operator's request. The initialization refusal in this
boot remains open.

## Bring-up sequence

1. **Qualify a console-only candidate.** Rehearse the exact EFI payload in a
   VM. Plan an attended native check for firmware framebuffer output and the
   first kernel console markers. Prove a return path to Ubuntu that works
   without the built-in keyboard or the USB log volume before attempting the
   one-shot boot. Capture the screen if the volume is unavailable.
2. **Qualify EHCI transport.** Start with controller ownership, bounded reset
   and transfer timeouts, and verified DMA retirement. Rehearse high-speed
   hub enumeration and read-only mass storage in QEMU. Exercise the
   full-speed topcase path behind the actual hub, including the required
   split transactions, before claiming keyboard support. Add UHCI companion
   support if an observed route requires it. Retest the existing xHCI path.
3. **Validate native input and workspace.** Verify the built-in keyboard,
   then the trackpad reports and pointer. Check the selected exFAT volume
   read-only first. Rehearse its MBR layout on a disposable image, then use a
   bounded test file to verify native write, sync, unmount and Ubuntu readback.
   Compare the pre-trial files and collect the exact release and boot evidence.
4. **Advance to desktop and individual providers.** Verify the 1280 × 800
   desktop, pointer and a saved-file round trip. Qualify brightness, analog
   audio routing, fan/temperature telemetry, battery/AC and networking in
   separate trials using the unit's captured resources for each match.

Before a physical trial, verify the staged manifest and exact VM result,
bind the USB UUID and recovery state in an ignored local run record, and
record the checks and one-shot Ubuntu return. The workspace identifies the
boot-time logging volume and is not a boot installer. [Ubuntu device-agent guide](../ubuntu-device-agent-setup.md)

## First console checkpoint

Release `c1a8e397a41363dc` was staged from clean source `74f803ff` using
`ram-usb-v4-console`. The exact EFI payload passed the GRUB and Limine
chainload, RAM filesystem, USB input and console rendering rehearsal.
A second Q35 rehearsal disabled USB and i8042, reached the 1280 × 800
console, and verified that GRUB cleared `next_entry` on ext4 before
chainloading. After stopping the guest from the host, the next VM boot
selected the Ubuntu default fixture without keyboard input.

In the attended native trial, the operator reached the SrvrOS terminal on
the internal panel. Keyboard input had no effect. This establishes native
console output and records an input failure for this release. The physical
framebuffer mode was not measured. The console-only release uses RAM storage
and left USB workspace persistence untested.

After Ubuntu returned, its boot ID had changed, GRUB's one-shot entry was
consumed, and the firmware entries matched the pre-install snapshot. The
installed EFI payload matched the staged manifest and the exFAT volume was
mounted at the expected location. The method used to return was not reported.
An independent local archive preserves the payload and return evidence
outside `build/`.

## EHCI controller probe

The kernel runs the probe only with `srvros.ehci=probe` and
`srvros.storage=ram`. It discovers EHCI PCI functions, validates the BAR and
capability layout, waits for firmware ownership, and resets each controller
after verifying it is halted. A successful probe leaves interrupts, bus
mastering, schedules and the configured flag clear, then releases its
temporary MMIO mapping. The probe refuses a retained BIOS semaphore. Hub
enumeration and transfer queues are later milestones. [Intel EHCI specification](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

Run the host fixture and staged VM rehearsals from the device checkout.
The EHCI rehearsal attaches two emulated controllers and adds the probe
option to a disposable copy of the plain console stage.

```sh
make -j2 test-ehci test-xhci
make -j2 hardware-stage
python3 tools/hardware_boot_smoke.py --ehci-probe
python3 tools/hardware_boot_smoke.py --console-check
```

Source `e062efae` staged as `321ddfa0f613f514`. With USB input and i8042
absent, QEMU exposed `8086:293a` and `8086:293c`. Both six-port controllers
reported OS ownership, halted reset completion and mapping retirement before
the kernel entered the console. The host fixture also covers a retained BIOS
semaphore, malformed capability data, stalled halt and stalled reset.
The xHCI host suite and staged console regression passed. The latter booted
the same kernel with USB input and the ordinary command line and produced
no EHCI probe output. This evidence is from the VM and host fixtures.

## EHCI high-speed enumeration

`srvros.ehci=enumerate` is a second RAM-only diagnostic. After the same
handoff and reset, it uses a single asynchronous endpoint-zero queue for
descriptor reads and no-data requests. It assigns addresses to high-speed
devices, reads their configurations and configures hubs to inspect connected
ports. A full-speed or low-speed child is disabled before the next address-zero
transaction. The bounded scan covers up to 16 devices, up to 16 ports per hub
and four downstream tiers. It disables the schedules and PCI bus mastering
before releasing the DMA page. An unproved halt retains the page.

The built-in QEMU hub is USB 1.1. The Q35 rehearsals use its high-speed tablet
model and a synthetic USB 2.0 hub attached over a local `usb-redir` socket.
A host fixture models a connected high-speed child, a full-speed child that
must be disabled, a malformed hub descriptor and a stalled reset. [QEMU USB emulation](https://www.qemu.org/docs/master/system/devices/usb.html) · [USB redirection protocol](https://github.com/SPICEorg/usbredir/blob/master/usb-redirection-protocol.txt)

```sh
make test-ehci hardware-stage
python3 tools/hardware_boot_smoke.py --ehci-enumerate
python3 tools/hardware_boot_smoke.py --ehci-hub
```

Source `b39cc8bc` staged as `12940cc028c792f5`. The tablet run assigned
addresses 1 and 2 on one controller and safely retired the other controller's
empty schedule. The redirected-hub run configured a three-port high-speed
hub, powered and read all three downstream ports, then addressed a second
root device. Both runs released the DMA pages after verifying controller
halt. The connected child and slower-port isolation evidence is from the
host fixture. The reset-only EHCI probe and ordinary xHCI console regressions
passed on the same staged kernel.

## EHCI full-speed split control

`srvros.ehci=split` enables full-speed endpoint-zero requests on the RAM-only
diagnostic. For a full-speed child directly behind a high-speed hub, the
asynchronous queue carries the parent hub address and the one-based port.
The driver reads the first eight descriptor bytes at address zero, adopts the
advertised endpoint-zero packet size, then keeps the same hub route through
`SET_ADDRESS` and the configuration reads. A failed request stops the scan
before another port can acquire address zero. The diagnostic defers low-speed
children and leaves any discovered full-speed hub's downstream ports alone.
[Intel EHCI specification, sections 3.6 and 4.12.1](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

```sh
make test-ehci hardware-stage
python3 tools/hardware_boot_smoke.py --ehci-split
```

The host fixtures model the routed control requests, malformed packet sizes,
and the schedule's completion and timeout behavior. The VM exercises the
split boot option with the redirected high-speed hub and a high-speed tablet.
QEMU's built-in hub is a USB 1.1 model, and the redirected high-speed hub has
empty ports, so the VM run reports zero split devices. [QEMU USB hub model](https://github.com/qemu/qemu/blob/v11.0.3/hw/usb/dev-hub.c)

Clean source `51d279b4` staged as `0f5d544618bece74`. Split-mode boot reached
the console, configured the three-port high-speed hub and enumerated the
tablet. Both controllers released their DMA pages. The manifest, host test
output and VM logs are in `build/review/macbookpro8-1-ehci-split-0f5d544618bece74/`.
The reset-only, high-speed tablet, high-speed hub and ordinary xHCI console
regressions passed on the same staged kernel.
The installed console release `c1a8e397a41363dc` matches its recovery archive.
Ubuntu is the GRUB default and `next_entry` is empty.

## Attended EHCI split trial

The native profile displays the checklist during startup and holds the final
result for a photo and manual power-button return. It stops before storage,
ordinary input initialization and userspace. Its empty initramfs fits the
candidate beside the known-good console release on the internal EFI partition.
The exact-stage QEMU test uses the high-speed hub fixture, saves a screenshot,
checks the on-disk GRUB selection was consumed and starts the same virtual disk
again to check the Ubuntu default entry. A QEMU rehearsal pass yields an
incomplete guest result because the native topcase and controllers are absent.

```sh
make -j2 build/kernel/srvros.elf
python3 tools/hardware_boot.py stage --ehci-split-trial
python3 tools/ehci_trial_smoke.py "$(cat build/hardware-boot/latest)"
python3 tools/ehci_trial_smoke.py "$(cat build/hardware-boot/latest)" --stall-hub-status
```

Clean source `8a51702a` produced release `91a277b2e434e4b3`. The exact VM
rehearsal verified the readable 1280 × 800 result screen, both emulated EHCI
scans, DMA retirement and the Ubuntu default marker on the second boot.
The ordinary xHCI console rehearsal passed on the same source. For that
trial, `c1a8e397a41363dc` was retained as the previous release. Both installed
manifests and the firmware boot order were verified. The local handoff is
`build/review/macbookpro8-1-ehci-trial-91a277b2e434e4b3/run.json`.

The native screen must show the expected `05ac:0245` topcase through
`0424:2513` port 2 with an 8-byte control packet and both Sandy Bridge EHCI
controllers completing the scan and releasing DMA. Photograph the full screen,
including the release and result, then use the power button to return to
Ubuntu. The on-disk run record binds the photo or result lines to the release
and Ubuntu boot state. A missing completion line leaves the test incomplete.

The operator transcribed the native screen from `91a277b2e434e4b3` as
`INCOMPLETE`. Both controllers reached reset-halted and released their DMA
pages. `00:1a.7` (`8086:1c2d`) completed its scan. It read the `05ac:0245`
topcase device and configuration descriptors through hub `0424:2513` port 2
with an 8-byte endpoint-zero packet, assigning address 3 through hub address
1. This verifies the split control path for the built-in topcase.

`00:1d.7` (`8086:1c26`) stopped before completing its scan. Ubuntu shows a
low-speed Apple IR receiver `05ac:8242` on port 1 of that controller's hub
and the high-speed flash drive `13fe:6700` on port 2. The first trial screen
recorded completion per controller, leaving its failing operation unknown.
The new Ubuntu boot ID, cleared `next_entry`, unchanged
firmware boot order, mounted exFAT workspace and both installed manifests
were verified. The transcript, verification and exact payload were archived
locally outside `build/` alongside the known-good console release.

### Scan failure details

The retry profile keeps the first unfinished operation for each controller
and shows the root port or hub address and port, the controller's `USBSTS`,
and the last sampled port status. Control-transfer failures also show the
USB address, request number and queue descriptor status bytes. The QEMU
stall fixture exercises this screen by returning a USB stall for hub port
2's status request. The retry preserves the same scan and DMA retirement
rules as the first run.

Clean source `00211b89` staged as `47632d5174507ea3`. In QEMU, the normal
fixture completed both scans. The forced stall reported hub 1 port 2 during
preflight, a qTD error on `GET_STATUS` and `USBSTS=0x8007`. Both rehearsals
released both DMA pages, showed the result screen and selected the Ubuntu
default fixture on the next boot. That release was installed for the retry
with `c1a8e397a41363dc` retained for recovery. The prior partial result and
its payload remain archived. The retry record and VM
evidence are in `build/review/macbookpro8-1-ehci-trial-47632d5174507ea3/`.

The operator's native result for `47632d5174507ea3` was `INCOMPLETE`. The
topcase descriptor reads and address 3 were reproduced on `00:1a.7`, and both
controllers released their DMA pages. `00:1d.7` reached hub address 1 port 2
and timed out on the initial `GET_DESCRIPTOR` at USB address 0. The Ubuntu
sysfs route ties that port to the attached `13fe:6700` workspace drive at
480 Mb/s.

The sampled hub port status `0x00100503` shows a connected, enabled, powered
high-speed port and a reset-change indication. `USBSTS=0xa005` shows that the
asynchronous schedule was active at the stop. The setup qTD completed with
status 0, while the requested data IN and status qTDs both remained active
(`0x80`) under the configured 100 ms completion limit. That locates the
timeout before address assignment. The reason the data phase stayed active
remains open. [USB hub status bits](https://github.com/torvalds/linux/blob/master/include/uapi/linux/usb/ch11.h) · [Intel EHCI specification](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

Ubuntu return was verified with a changed boot ID, consumed `next_entry`,
unchanged firmware boot order and matching manifests for the trial and the
previous console release. The workspace mounted in Ubuntu. The retry's
transcript, VM evidence and exact payload were archived outside `build/`
alongside the earlier trial.

### Initial flash descriptor timing trial

USB 2.0 allows up to 500 ms for the first data packet of a standard control
read and another 50 ms for status after the last packet. The trial allows
600 ms including host margin for the initial one-packet device descriptor
read at address zero on the captured `00:1d.7` high-speed hub route. The last
port, hub identity and exact request must match. Other requests use the
existing 100 ms limit. [USB 2.0 specification, section 9.2.6.4](https://www.usb.org/document-library/usb-20-specification)

The screen prints the measured wait and received byte count for that request.
It also reports the active EHCI queue overlay token when a transfer fails.
Trial acceptance requires both retired controllers, the topcase descriptors,
an 18-byte flash device descriptor and assigned-address enumeration of the
expected `13fe:6700` flash drive. The trial stops before storage I/O.

Clean source `88d26f10` staged as `7c634ec7f48273c4`. The host fixture passed
an 18-byte response after a simulated 120 ms on the selected route, stopped
at 100 ms on another port and bounded a stalled selected request at 600 ms.
It verified schedule disable after each case. Both exact-stage QEMU recovery
rehearsals and the ordinary EHCI hub regression passed. The trial record and VM
evidence are in
`build/review/macbookpro8-1-ehci-trial-7c634ec7f48273c4/`.

The operator's native result for `7c634ec7f48273c4` was `PASS`. The 18-byte
flash device descriptor completed after 493,456 µs of the 600,000 µs wait.
The flash drive `13fe:6700` reached USB address 2 on `00:1d.7`. The `05ac:0245`
topcase again reached address 3 through hub `0424:2513` port 2 with an 8-byte
endpoint-zero packet on `00:1a.7`. Both controllers completed their scans
and released DMA. The measured flash wait exceeded the former 100 ms limit.

The Ubuntu boot ID changed, GRUB consumed the one-shot entry, and the flash
workspace mounted on the same route. Firmware boot order and the installed
manifests matched preflight, including previous console release
`c1a8e397a41363dc`. The native transcript, verification and exact payload were
archived outside `build/`.

## Attended EHCI keyboard trial

The captured `05ac:0245` configuration identifies boot keyboard interface 0,
interrupt IN endpoint `0x83`, 10-byte maximum packets and an 8 ms interval.
The RAM-only `--ehci-keyboard-trial` profile verifies the topology and this
interface before issuing `SET_CONFIGURATION` and `SET_PROTOCOL` for boot
keyboard mode. It expects the 8-byte boot report. [USB HID specification](https://www.usb.org/document-library/device-class-definition-hid-111)

The periodic queue uses the existing hub and port route, start-split in
microframe 0 and complete-splits in microframes 2–4. Before reusing the queue
for another report, the driver verifies that the periodic schedule stopped
and preserves the endpoint's data toggle. It releases the frame list and
control page after verifying controller halt and PCI bus-master disable.
[Intel EHCI specification](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

At the guided `srv>` prompt, type `echo Ehci-42` on the built-in keyboard
and press Return. The trial accepts that line through the kernel's HID
keyboard queue and displays the result for a photo and power-button return.
The prompt accepts this one test line. The keyboard interval is bounded by
90 seconds. The trial leaves the trackpad and flash data interfaces idle.

```sh
make -j2 build/kernel/srvros.elf test-ehci test-keyboard
python3 tools/hardware_boot.py stage --ehci-keyboard-trial
python3 tools/ehci_trial_smoke.py "$(cat build/hardware-boot/latest)"
```

Clean source `520f663f` staged as `241ed4d3abb212d7`. The host fixtures
verified the captured interface match, frame list, split queue layout,
alternating data toggle, 8-byte completion, timeout and DMA retention when
schedule stop is unproved. The kernel's keyboard decoder suite passed.
Both exact-stage QEMU rehearsals verified the held screen and one-shot
return with the native topcase absent. The ordinary xHCI console regression
passed on the same source. The keyboard release was installed for the trial,
with previous console release `c1a8e397a41363dc` and Ubuntu recovery verified.
The trial record and VM evidence
are in `build/review/macbookpro8-1-ehci-trial-241ed4d3abb212d7/`.

The native screen for `241ed4d3abb212d7` reported `INCOMPLETE`, with a typing
timeout, zero completed reports and zero report bytes. In this source the
timeout follows successful keyboard configuration, selection of boot
protocol and acknowledgement of the periodic schedule enable. The raw qTD
transcription is ambiguous. Its likely reading, `0x000a0d80`, matches the
prepared active 10-byte interrupt IN descriptor. The source qTD is written
back on retirement, so the receipt leaves queue traversal and the device's
response unresolved. [Intel EHCI specification, section 4.10](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

Both scans completed and both controllers released DMA. The topcase again
reached address 3, and the flash drive returned 18 bytes in 493,359 µs and
reached address 2. Ubuntu returned with a changed boot ID and the GRUB
one-shot entry consumed. The topcase's Linux route and interface, installed
manifests, firmware boot order and exFAT mount were rechecked. The transcript
and exact payload were archived outside `build/`.

### Keyboard periodic polling evidence

Ubuntu's running `00:1a.7` schedule shows the same full-speed keyboard
endpoint 3 IN, 10-byte packet size, 8 ms interval and split mask `0x1c01`.
The trial keeps those settings and requests a keyboard report every 32 ms
using report ID zero. The result screen records whether `SET_IDLE` completed,
and the time to the first 8-byte report if one arrives. Typing still requires
the expected line from the built-in keyboard. [Linux EHCI schedule format](https://raw.githubusercontent.com/torvalds/linux/master/drivers/usb/host/ehci-dbg.c) · [USB HID specification, section 7.2.4](https://www.usb.org/document-library/device-class-definition-hid-111)

On failure the screen shows frame-index movement, whether the controller's
current pointer matched the submitted qTD, the observed complete-split
progress bits, and the last `USBSTS`, source qTD and queue-head overlay
tokens. Samples are taken before schedule disable. Frame-index movement
verifies the controller clock, while the current qTD pointer and overlay
give separate evidence of traversal. The sampled progress mask can miss
transient microframes. [Intel EHCI specification, sections 2.3.4 and 4.12.2](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

Clean source `d384a8c2` staged as `0c509b2b181ab24d`. The host fixture
distinguished a moving frame index with an untouched queue from a loaded
qTD with observed complete-split progress. It also verified frame-index wrap,
8-byte report completion and schedule retirement. Both exact-stage QEMU
rehearsals and the ordinary xHCI console regression passed. Release
`0c509b2b181ab24d` is installed with `c1a8e397a41363dc` retained for recovery.
The run record, VM evidence and Ubuntu periodic schedule snapshot are in
`build/review/macbookpro8-1-ehci-trial-0c509b2b181ab24d/`.

The operator reported that the built-in keyboard worked properly on this
native retry. This records working input in the attended profile. The final
screen, exact test line, HID idle acknowledgement, report timing and DMA
retirement counters remain unverified for this run. The observation alone
leaves the reason for the earlier timeout unresolved. Ubuntu returned with
a changed boot ID and consumed `next_entry`. The installed manifests,
firmware boot order and exFAT mount matched preflight. The exact payload and
operator observation were archived outside `build/`.

### Resident keyboard in the regular monitor

Stage with `python3 tools/hardware_boot.py stage --ehci-keyboard-monitor`.
This RAM-only profile claims the captured topcase on `00:1a.7` and enters the
regular monitor. The periodic worker feeds the existing boot-HID decoder.
The on-screen checklist exercises mixed-case echo, history recall, cursor
movement, Delete and `reboot`. Reboot requires halted schedules, bus mastering
disabled, cleared schedule bases and verified MMIO release. The result line
shows completed reports and resource retirement for a photo before reset.
If that verification fails, use the power button to return to Ubuntu.

`tools/ehci_trial_smoke.py <release>` exercises the exact monitor payload
through serial in Q35, including echo, history editing, ACPI reset and
consumption of the one-shot entry. The host EHCI fixture checks periodic
report completion and halt-before-release behavior. The physical keyboard and
native reset are assessed from the installed laptop release.

On release `d7990a68cf4130ff` the operator confirmed the built-in keyboard,
command execution, and clear case distinctions in the regular monitor.
Ubuntu returned with a different boot ID and an empty GRUB one-shot entry.
The installed and recovery manifests, EFI boot order and exFAT mount matched.
The operator report did not specify history editing, the return method or
the EHCI release counters. The exact payload and evidence were archived
outside `build/`.

### Wellspring5 trackpad reference

Ubuntu on this unit binds `bcm5974` to interface 2, while the trackpad data
arrives on endpoint `0x81` of interface 1. The captured descriptor reports
64-byte full-speed interrupt packets and a 2 ms interval. Ubuntu's live EHCI
schedule uses a 1 ms interval and start/complete mask `0x1c01` on that endpoint.
Wellspring5 reports use a 30-byte type 2 header and 28 bytes per finger slot,
so a report with two slots spans multiple packets. Mode selection uses an
eight-byte HID feature read/modify/write with value `0x0300` and index 0.
[Linux bcm5974 reference](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/input/mouse/bcm5974.c)

Native release `b47bd60baf233327` scanned the controller and reached the
gesture prompt. The capture timed out with zero decoded frames, 656 invalid
transfers, one ignored two-byte response and an eight-byte last transfer.
The recorded restoration, DMA and MMIO release checks passed. A changed
Ubuntu boot ID and empty GRUB one-shot entry confirmed return. The photo
and verified payload are archived outside `build/`.

Release `a4e7cdb4482bf5c1` read back feature mode 1 after writing it from
mode 8. The longest completed transfer was eight bytes. All 282 eight-byte
transfers failed type 2 decoding, and none started with report ID `0x44`.
The last started with byte 2. The run restored the mode and released DMA and
MMIO. Ubuntu returned and the result and payload were archived.

The next work block adds result logging to the attached exFAT workspace
through the captured high-speed EHCI storage route on `00:1d.7`. It reuses
the shared USB Bulk-Only Transport and workspace code, selects the prepared
volume by UUID and saves one release-named file. Checks cover bulk transfer,
volume selection, flush, unmount and controller retirement. Trackpad report
decoding remains open.

Prepare the workspace marker in Ubuntu and stage with
`python3 tools/hardware_boot.py stage --ehci-trackpad-trial --ehci-trackpad-log-volume <exFAT UUID>`.
Rehearse with `python3 tools/ehci_trial_smoke.py <release>`. The attended trial
opens the checklist before polling. It requires a change in one-finger
coordinates, two simultaneous active contacts, and both click transitions
within 60 seconds. A pass also requires the feature mode to be restored and
both the controller DMA and MMIO access to be retired. The keyboard monitor
payload remains available in the recovery archive.

The trial saves `srvros-work/ehci-trackpad-<release>.txt` and requests a restart
after verified unmount and controller retirement. The result screen stays
visible on a save or retirement failure. A power cycle then returns to Ubuntu.

After the `284f78cff9438b7a` one-shot, Ubuntu returned and mounted the
expected workspace with its marker intact. The release-named report was
absent. The installed payload and recovery images matched their manifests.
The native stopping point and restart method remain unknown. The return
record is archived with the exact payload.

On the second attempt with `284f78cff9438b7a`, the photo shows that SrvrOS
registered the USB disk with 512-byte sectors and found its MBR partition.
The exFAT directory scan failed before the workspace mounted. The controller
released DMA, and a manual power cycle returned to Ubuntu. The trackpad
produced 323 eight-byte responses and zero decoded reports. The photo and
return verification are archived separately from the first attempt.

The SrvrOS parser reproduced the failure in Ubuntu. A read-only metadata
capture verified an exFAT cluster count of 473,128, a 59,142-byte allocation
bitmap with one byte beyond the 59,141 required bytes, and a matching up-case
table checksum. The FAT chains for both streams terminate after one cluster.
The parser now accepts reserved bitmap padding and validates the declared
stream chain. The existing writable limit remains one bitmap cluster.
The host regression checks file creation and remount with the reserved bytes
preserved. Native report save and automatic return still need verification.
[exFAT allocation bitmap specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#715-allocation-bitmap)

The updated Ubuntu checker read all 100 files with zero allocation errors.
The separate writable check found a primary volume dirty flag with valid,
matching boot regions. Offline exfatprogs cleared the flag and normalized
the bitmap length. The read-only writable check then passed with zero writes.
Native release `a730b00708a8820b` mounted the USB workspace and saved a
4,047-byte report that Ubuntu read back and verified by release and hash.
The saved log ends before the final unmount and reset messages. Ubuntu
booted with the one-shot entry consumed. The trackpad still timed out with
zero decoded reports, 242 eight-byte transfers and one ignored response.

Source `09b320bb`, release `1ee4e80c7cbf4111`, selects and verifies hub
alternate 1 before enumerating the topcase. It arms six interrupt transfers per
periodic schedule window. Short packets advance to the next report buffer,
and the next batch keeps the endpoint data toggle after the schedule stops.
The USB report adds the batch and completion counts and the latest eight-byte
sample. Local EHCI tests exercise the positive and refusal cases. The exact
release passed the VM's checklist, absent-controller and one-shot Ubuntu
default checks. Trackpad gestures require the attended native trial. A clean
Ubuntu `sync` and unmount cleared the mounted flash drive's dirty flag, and
the offline writable check passed with 102 files and zero writes or allocation
errors.

The native `1ee4e80c7cbf4111` run verified hub alternate 1 and completed
234 interrupt transfers across 40 schedule windows. One short response was
ignored and 233 eight-byte responses were rejected. The last was
`02 00 00 00 00 00 02 00`. Feature mode changed from 8 to 1 and was restored.
The capture decoded zero type 2 reports and timed out with motion, contacts
and clicks unverified. The topcase controller released DMA and its MMIO map.

Ubuntu read back the 4,178-byte report from the expected USB volume and
confirmed the consumed one-shot and unchanged EFI entries. The current,
console recovery and keyboard monitor payloads matched their manifests.
USB contents matched the pre-trial copy except for the new report and the
expected sidecar addition and workspace marker access time. The read-only
offline allocation check passed for all 103 files. The saved report ends
before the guest log controller's final teardown and reset messages.
The run record, report and exact payload are archived outside `build/`.

On the same topcase and controller, Ubuntu `7.3.0-11-generic` completed 988
gesture transfers on endpoint `0x81`. All began with `0x74`, with 718 reports
of 58 bytes and 270 of 86 bytes. SrvrOS's Type 2 decoder accepted all 988 in a
host replay, with one-finger motion, two active contacts and both click
transitions. The native eight-byte sample was absent from the Ubuntu capture.
The usbmon reader reported zero lost events.

In five Ubuntu starts, the mode-1 feature write completed 33–1,765 µs before
the first 478-byte interrupt submission, with a median of 156 µs. The saved
SrvrOS release performs an extra feature readback, programs the frame list
and renders the readiness prompt before enabling its periodic schedule.
Its mode-to-arm interval was unmeasured. The next native check should isolate
that sequence and log the arm timing and first endpoint payload. The Ubuntu
comparison was saved beside the original report on the USB workspace and the
offline check passed with 104 files. [Linux usbmon ABI](https://docs.kernel.org/usb/usbmon.html)

The timing candidate is source `5b446f31`, release `cacf59a022b41970`.
It prepares the first batch and displays the complete checklist before the
mode write. The timing starts when software observes the control transfer
complete. The report records the periodic enable request, the controller's
acknowledgment and the first observed interrupt completion relative to that
point. It includes the entire first payload as ordered hex chunks, then uses
the normal mode restoration and USB save path. The EHCI host tests verified
the request order and the retained multi-packet payload. The exact VM payload
passed the checklist, absent-controller and Ubuntu-default rehearsal.

The native `cacf59a022b41970` trial requested periodic enable 13 µs after
observed completion of the mode write. The controller acknowledged at 15 µs.
The first interrupt completion arrived at 474,169 µs and carried `60 02`,
which the decoder ignored as a two-byte response. The following 366
completions carried eight bytes, with the last sample matching the previous
trial. The post-capture feature read was 1, the original mode was restored,
and the topcase controller released DMA and its MMIO map. The trial timed
out with zero decoded Type 2 reports. Prompt rendering and intermediate
feature readback were eliminated from the mode-to-arm interval in this run.

The release-matched 4,363-byte USB report was verified after remount in
Ubuntu. The one-shot was consumed, firmware entries matched, and the
installed and recovery payloads verified. Prior file contents were preserved,
with the expected sidecar updates for the two reports and the workspace
marker access time. The offline allocation check passed with 105 files and
zero writes. The saved log ends before the final USB teardown and reset.
The exact payload and return evidence are archived outside `build/`.

The Ubuntu initialization comparison for `cacf59a022b41970` used live
topcase reauthorization on the same hub and EHCI controller. After
`SET_CONFIGURATION 1`, `usbhid` sent `SET_IDLE` and retrieved the 156-byte
keyboard report descriptor on interface 0, sent two keyboard output report
requests, then sent `SET_IDLE` and retrieved the 27-byte vendor report
descriptor on interface 1. The mode-1 write completed about 232 ms after
configuration in this sample. The native profile skips these HID requests.
The live Ubuntu schedule confirms 64-byte packets on endpoint `0x81` and
hub port 2, with a 1 ms period and mask `0x1c01` matching native construction.
Ubuntu resubmits its interrupt URB on the running schedule. Native keeps
six queued descriptors per window and checks schedule stop before rearming.
The usbmon evidence covers USB requests at the host driver boundary.
[Linux usbhid](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/hid/usbhid/hid-core.c) · [Linux EHCI queue code](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/usb/host/ehci-q.c) · [Linux usbmon ABI](https://docs.kernel.org/usb/usbmon.html)

Interface 2 declares a four-byte mouse report including ID `0x02`. Interface
1 declares a 512-byte HID input report including ID `0x44`. A separate Ubuntu
probe confirmed mode 8 and saw no completed interrupt payloads on `0x81`
or `0x84` in 20 seconds using their descriptor intervals. Ubuntu restored
the input drivers and mode 1, with valid Type 2 reports during the monitor
window. The source of the native eight-byte frames remains open. The
comparison report is saved on the USB workspace. The read-only allocation
check passed with 106 files, and the raw Ubuntu traces are archived locally.

The HID replay trial starts from `2ab5c95d` on the `61111819` base. A second
Ubuntu capture verified that both keyboard output requests carry `01 00`.
The native candidate replays the six observed HID requests and their
configuration-relative submission offsets before reading and writing the
trackpad feature report. It records partial progress and the timing of the
mode write. The scoped MacBookPro8,1 trial retains the existing EHCI interrupt
queue, gesture checks and USB report path. The open PRs and Pro11,2 branch
have no overlapping EHCI work. The next checkpoint is the exact VM rehearsal
and an attended one-shot with the same flash drive and Ubuntu recovery.

Source `7365fd5d`, release `c1a658684ae3c9e1`, is the HID replay candidate.
It uses the captured submission times as minimum offsets after
`SET_CONFIGURATION 1`, including the 247,973 µs offset for the feature
read. The host fixture verified all request fields and both keyboard output
payloads, and verified that a short HID descriptor response stops the trial
before the mode switch. The exact VM payload passed the checklist,
absent-controller and Ubuntu-default rehearsal with the USB report enabled.

Native release `c1a658684ae3c9e1` completed all six replay requests. It
requested the feature read at 247,975 µs and completed the mode write at
248,783 µs after configuration. The EHCI queue became active 5 µs after
the write. The first transfer completed at 224,369 µs with `60 02`, matching
the previous trial's first payload. The other 386 completions were eight
bytes each, with the same final sample as before. The gesture capture timed
out with zero decoded Type 2 reports. The post-capture feature read was 1,
the original mode was restored, and the controller released DMA and MMIO.

Ubuntu verified the 4,507-byte USB report, consumed one-shot, current and
recovery payloads, and unchanged firmware entries. The existing USB file
contents were preserved apart from the expected sidecar updates. The
read-only allocation check passed with 107 files and zero writes. The saved
report stops before the final USB controller retirement and reset. The
native evidence, exact payload and Ubuntu replay input are archived together.

The feature and queue comparison starts from `de014e05` on the `61111819`
base. It captures the complete native feature reports before and after the
mode switch and restoration, plus queue-head and qTD progress through the
first batch. It compares them with the archived Ubuntu USB trace and a live
Ubuntu EHCI queue sample. The native changes are observations within the
existing trackpad trial and its fixtures. The HID replay, interrupt queue
and USB write path retain their timing and configuration. No overlapping EHCI
work was identified in the open PRs. The next checkpoint is the exact-stage
VM rehearsal and an attended one-shot with USB collection.

Source `c1f1cbb8`, release `22f0ade122d96d67`, is the instrumented
candidate. Its exact VM payload passed the checklist, absent-controller and
Ubuntu-default rehearsal. The host fixture verified full mode byte retention
and split-state, token and link sampling for a multi-packet transfer. The
live Ubuntu reference retained 12 retired qTD snapshots from endpoint
`0x81`. Each completed 58 of 478 requested bytes, with matching qTD and
overlay tokens, alternating toggle and C-progress `0x04`. The earlier USB
trace supplies the complete mode payloads `08 05 00 00 00 00 00 00` and
`01 05 00 00 00 00 00 00`. The Ubuntu reference was saved to the flash
drive alongside the path reserved for the upcoming native report.

Native `22f0ade122d96d67` verified the full before, mode-write, post-capture
and restoration feature payloads against Ubuntu. The endpoint and capability
words agreed after masking the assigned device and hub addresses. All six
first-batch qTDs retired with matching overlay tokens, expected current and
next links, alternating toggles and sampled C-progress `0x04`. Their actual
lengths were 2, 8, 8, 8, 8 and 8 bytes. Each of 12 sampled Ubuntu qTDs
completed 58 bytes from the same 478-byte request. The native capture timed
out with zero decoded gestures and 649 rejected transfers. The source of
the short completions remains open. Linux reuses a running queue head and
resubmits through a dummy descriptor. That queue behavior remains a
separate variable from the measured feature payload and endpoint settings.

The native report, Ubuntu reference and comparison were verified on the
flash drive after remount. The offline allocation check passed for all 110
files with zero writes. The one-shot was consumed, Ubuntu returned, and the
installed and recovery payloads matched. The native report ends before the
USB controller's final teardown and reset. The evidence and exact payload
are archived outside `build/`.

The single-request trial starts from `1a3c2da6` on the `61111819` base.
The existing HID replay and mode timing stay in place. The candidate keeps
one periodic queue head linked, with one active interrupt request followed
by an inactive dummy qTD. On verified retirement it publishes work through
the old dummy while preserving the controller-owned overlay and toggle.
The changes are scoped to the trackpad trial and its fixtures. The current
open PRs have no overlapping EHCI changes. The next checkpoint is an exact
VM rehearsal and an attended one-shot with USB logging and Ubuntu recovery.
[Linux EHCI queue publication](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/usb/host/ehci-q.c)

Source `1462416f`, release `bc98fee76d0e18c3`, is the staged running-queue
candidate. It records submissions, completions and the first six queue
snapshots. The host fixture verified one schedule start and stop across
11 requests, retained the hardware toggle, and refused to republish when
the fake controller advanced into the inactive dummy. The exact VM payload
passed the checklist, absent-controller and Ubuntu-default rehearsal.

Native `bc98fee76d0e18c3` completed all six HID requests and retained the
same complete mode payloads as the previous trial and Ubuntu. The periodic
queue started once and recorded 384 resubmissions and 384 completions, with
the final request still pending at the gesture deadline. The first six
snapshots verified alternating descriptors and data toggles, matching qTD
and overlay tokens, and C-progress `0x04`. Their transfer lengths were
again 2, 8, 8, 8, 8 and 8 bytes. The full run logged one ignored response,
383 rejected eight-byte transfers, and zero decoded gestures. The original
mode and topcase DMA and MMIO retirement verified.

Ubuntu returned with the one-shot consumed and the EFI entries unchanged.
The 6,275-byte native USB report matched its archived copy. The read-only
allocation check passed for all 111 files, and prior file contents were
preserved with the expected sidecar updates. The exact payload and report
are archived outside `build/`. The saved report stops before USB teardown
and reset.

Source `10917f4d`, release `d4a82982f7c2bccb`, adds the raw endpoint trace.
The host fixture verified separate queues on `0x81` and `0x84`, their
payload lengths, bounded sample logging, and refusal to run the mouse phase
after an unsafe primary queue state. The exact stage passed its QEMU
checklist, absent-controller, and Ubuntu-default rehearsal.

Ubuntu USBFS and usbmon captured matching mode-1 completions on this unit.
Endpoint `0x81` returned 765 58-byte and 561 86-byte frames, all beginning
with `0x74`, at the descriptor interval of 2 ms. All 56 requests on endpoint
`0x84` timed out at the 8 ms descriptor interval. The full feature payload
and all three driver bindings were restored. The existing Ubuntu
`bcm5974` 1 ms baseline shows the same frame lengths and leading byte on
`0x81`. Both fresh traces and their summary were saved to the USB workspace.

Native `d4a82982f7c2bccb` recorded one `60 02` completion about 224 ms after
the mode write and 308 eight-byte completions on endpoint `0x81`. The 15
logged eight-byte samples and the final response all start with `0x02`.
The queue head identifies endpoint 1 with a 64-byte max packet, and the
first six retired qTDs and their overlays agree on the transfer lengths.
The separate 15-second `0x84` phase had no completions. This agrees with
Ubuntu on `0x84`, whose mouse descriptor specifies a four-byte ID `0x02`
report. The native trial recorded zero Type 2 gestures and verified the
feature restoration and topcase DMA and MMIO release.

Linux `bcm5974` reports the same eight-byte symptom when the mode switch
is sent before the interrupt control response. It returns to normal mode,
waits about 1 ms, and retries Wellspring mode. That is the leading explanation
for the native `0x81` traffic even though the feature readback says mode 1.
The host-side traces identify the queue that completed and leave the physical
packet source and device state unverified. A bounded native retry after the
`60 02` response can test whether the Linux recovery restores Type 2 frames.
[Linux mode recovery](https://github.com/torvalds/linux/commit/fc1e8a6f129d87c64ac8e58b50d9dfa66217cfda)
[USB trace scope](https://docs.kernel.org/usb/usbmon.html)

Ubuntu returned and consumed the one-shot. EFI entries and the installed
and recovery payloads verified. The 8,434-byte native result and comparison
matched their archive copies after USB remount. The read-only allocation
check passed for 115 files with zero writes. The saved native report ends
before the flash controller's final teardown and reset.

Source `6489a417`, release `53177189337d3d65`, is the staged mode-recovery
trial. The host fixture verified the normal and Wellspring control writes,
the minimum 1 ms wait, queue resumption with the controller's data toggle,
and restoration on timer failure. The exact-stage VM verified the operator
checklist, absent-controller handling, and Ubuntu-default rehearsal. The
flash report records the trigger, feature writes, wait and resume timing,
and the first decoded Type 2 frame after the retry if one arrives.

Native `53177189337d3d65` passed. The initial `0x81` queue again completed
`60 02` followed by an eight-byte `0x02` frame. The diagnostic paused the
queue and wrote mode 8, recorded a 1,014 µs wait, read back mode 8, and
wrote mode 1. The two writes completed 1,605 µs apart. The same queue
resumed 14 µs after the retry, and its first Type 2 frame arrived 5,502 µs
after the Wellspring write. It was 58 bytes and began with `0x74`, matching
the Ubuntu reference. The first six transfer lengths were 2, 8, 58, 58, 58
and 58 bytes with alternating qTDs and data toggles across the retry.

All 224 post-retry completions decoded as Type 2, with a maximum length of
86 bytes. The trial verified one-finger motion, two contacts, and click press
and release. Endpoint `0x84` had no completions. The full original feature
payload and topcase DMA and MMIO release verified. This accepts recovery and
gesture decoding in the isolated native trial.

Ubuntu returned with the one-shot consumed, EFI entries unchanged, and the
installed and retained payloads verified. The 7,667-byte native report and
the recovery comparison matched the archived copies after USB remount. The
offline allocation check passed for 117 files with zero writes. The saved
native report ends before flash-controller teardown and reset.

Source `738795d8`, release `c431c3461b31a9a3`, is the desktop pointer
candidate. The boot option `srvros.ehci=trackpad` selects the resident
input worker for the captured topcase during regular startup. The attended
profile connects the same worker to the regular compositor and launches
the checklist in Text. It checks cursor movement on both axes, mouse
activation of maximize and restore, and pointer selection of Return to
console. A 90-second deadline allows an incomplete run to save its report.

The host fixtures passed EHCI transfer, Type 2 pointer, ordered button-edge,
and mouse ABI checks. The exact QEMU boot opened the in-guest checklist at
1280 × 800. Virtual PS/2 input exercised both axes, both title-bar actions,
and the return menu. The absence of the captured topcase kept the VM's
overall hardware result incomplete. The reboot rehearsal returned to the
Ubuntu default. The attended image carries the compositor, Text and its
required helpers in RAM so the recovery image still fits on the EFI volume.

The operator confirmed the built-in trackpad worked in native desktop
`c431c3461b31a9a3` and reported a flash save verification warning on exit.
Ubuntu returned and consumed the one-shot, and EFI entries and retained
payloads verified. The report was absent. All 117 workspace files matched
their backups, and offline allocation and writable checks passed. The
operator observation does not supply the compositor or retirement counters.

The exact staged initramfs and current USB partition reproduced the save
blocker in a read-only host replay. The desktop bootstrap allocated inode 2
to `/fat`, while the workspace sidecar already assigned it to `/work`.
Additional boot inode numbers overlapped saved metadata. Writable admission
refused even though exFAT allocation passed with zero I/O writes or flushes.
The shared fix in PR #554 assigns generated boot IDs to a separate range.

The merge at `3849ae3b` brings in the upstream boot-inode fix. The read-only
replay found eight collisions with the original code and zero after the
merge. Writable admission passed with the workspace metadata unchanged.
Release `0a4b6e655574e009` passed the host regressions, the exact desktop VM
rehearsal, and writable admission for the current 118-file USB workspace
using its exact staged initramfs. The previous run's Ubuntu return note is
on the flash drive. The native retry will check the release-bound desktop
report after the pointer and click checklist.

The native desktop retry on `0a4b6e655574e009` passed the compositor
checklist at 1280 × 800. The built-in trackpad moved the pointer on both
axes, maximized and restored the Text window, and selected Return to console.
The driver recovered Wellspring mode after a 1,013 µs wait. It recorded 910
valid reports, 691 motion events, and four presses and four releases. It
restored the original mode and released the topcase resources. The aggregate
marker was `INCOMPLETE` because the driver counted one malformed frame. The
runtime report does not include that frame's length or bytes.

The operator confirmed the trackpad and reboot worked. Ubuntu read back the
21,272-byte native report, consumed the one-shot, and verified the retained
EFI and recovery payloads. The offline allocation check passed for all 119
files with zero writes. The previous user files matched their backups, and
the flash-drive report matched its archived copy after remount. The one-time
Ubuntu verifier removed its own startup entry after collecting the report.

Source `89c9a802`, release `e064782e489c1a51`, retains the first four
malformed desktop completions in full and counts further rejects. The report
records the completion ordinal, length and bytes, the observation time in
the worker, time since queue activation and the previous valid frame, and
the EHCI frame index and descriptor tokens. The host fixture verifies that a
478-byte sample survives buffer reuse and that further rejects cannot
overwrite it. The exact desktop VM rehearsal and read-only USB admission
with all 119 files passed. The native run uses the same Text checklist.

Native `e064782e489c1a51` passed the desktop checklist and the strict
transport check. The initial eight-byte response triggered the expected
Wellspring recovery, with a 1,013 µs wait. The driver recorded 1,153 valid
reports, 841 motion events, five presses and five releases. The malformed
trace had zero samples and zero omissions. The isolated malformed frame
from `0a4b6e655574e009` did not recur. The original mode and topcase resource
release verified.

The 21,347-byte native report matched its archived copy after Ubuntu
returned and the USB was remounted. The read-only allocation check passed
for 120 files with zero writes, and prior user files were preserved. The
one-shot was consumed and the retained recovery payloads verified.

The combined input candidate uses the same `srvros.ehci=trackpad` boot
option for the regular desktop. One controller worker owns `0x81` and
`0x83`. The keyboard uses one frame per eight and the trackpad uses the
other seven, giving their shared transaction translator disjoint split
windows. Configuration replays the existing HID setup, selects keyboard
boot protocol and enters Wellspring mode. The v2 attended profile opens
Text, checks typing into Find, and exercises pointer motion, maximize,
restore and the return menu. The native report counts keyboard edges and
invalid reports alongside the existing pointer, mode and retirement data.

Source `9020482e`, release `159368a637be087a`, passed the EHCI and pointer
host fixtures and the exact-stage QEMU checklist with virtual PS/2 input.
The rehearsal showed `Ehci-42` in Text's Find field, completed both
window actions, and selected Return to console before restoring the Ubuntu
default. The stage passed read-only writable admission for all 120 files
on this unit's flash workspace. The release is installed for a one-shot
native trial with the working console and standalone keyboard retained.

In the native `159368a637be087a` run, the operator confirmed the built-in
keyboard worked in the desktop and the trackpad did not. Ubuntu returned
and consumed the one-shot. The automatic verifier and a subsequent remount
both found the release report missing. All 120 pre-trial files matched
their backups, offline allocation passed with zero writes, and the EFI
entries and retained releases verified. The combined trial is incomplete.
Trackpad queue progress, topcase retirement and the point at which the
native save stopped remain unknown.

The v3 topcase diagnostic keeps the failing combined schedule and HID
sequence for comparison. It logs both periodic queue heads, the last
observed descriptor state and the first six request lifetimes per endpoint.
The trackpad samples record the wire report ID. The keyboard samples
record lengths and queue progress only. The startup snapshot uses the
separate flash controller before the desktop opens. Exit attempts a new
flash report after the topcase worker retires. Control+Alt+Escape and the
90-second deadline both reach cleanup even when the pointer is unusable.

Source `273f99a1`, release `3388f68afd5dc712`, passed the host fixtures
and two exact-stage QEMU rehearsals. The regular checklist completed in
one run. The other exercised the keyboard-only exit, reached the final
save attempt and returned to the Ubuntu default fixture. The VM recorded
both save attempts with the captured flash controller absent. The exact
RAM image passed write admission against this unit's 120-file workspace.

Native release `3388f68afd5dc712` passed with both built-in inputs. At
startup `0x81` had returned the two-byte `0x60` status packet and still had
an active request. `0x83` had an active request and no completion before
typing. The first touch produced an eight-byte `0x02` response at 4.894 s
after queue activation. The existing recovery restored normal mode, waited
1,014 µs and reselected Wellspring mode. The next observed completion was
a 58-byte `0x74` report at 4.904 s. Both queues continued to make progress
with the same frame allocation as the previous trial. The added startup
observations and flash save changed when Text opened. The earlier
trackpad failure did not recur and its cause remains unverified.

The desktop recorded 1,285 valid trackpad reports, 980 motion events,
six click presses and releases and zero malformed frames. The keyboard
returned 22 valid reports and eight press and release transitions. Typing
into Find, maximize, restore and pointer exit all passed. Mode restoration
and topcase retirement verified. Ubuntu read back the 11,044-byte startup
report and the 31,676-byte final report, then verified both hashes after
remount. All 120 prior files were preserved apart from the expected
metadata update. The offline allocation check passed for 122 files with
zero writes. Ubuntu consumed the one-shot and the retained payloads
verified.

Repeat boot 1, `eef462bd3beddb22`, reproduced the unresponsive trackpad
with the same payload and both USB reports intact. `0x81` completed 537
requests and returned zero valid pointer reports. The first eight-byte
`0x02` packet triggered the same mode retry as the passing release. Its
next completion was another eight-byte `0x02` packet. At the same point,
the passing release returned a 58-byte `0x74` report. The failure recorded
535 rejected packets and 34 valid keyboard reports. The first keyboard completion came
after the failed mode recovery. The keyboard-only exit, resource retirement,
and readback of both files verified. The offline allocation check passed
for 124 files with zero writes.

Repeat boot 2, `71e6c80c0828b18a`, passed the desktop checklist. The
trackpad returned one more eight-byte `0x02` response after the retry, with
the same sample bytes as the first post-retry response on boot 1. The next
completion was a valid 58-byte `0x74` report. The trace ended with 1,058
valid trackpad reports and 22 valid keyboard reports. The single rejected
frame kept the strict aggregate result incomplete. Both USB reports and
the 126-file offline allocation check verified.

Repeat boot 3, `e55e9ccf651097bb`, passed the desktop checklist and strict
transport checks. The first post-retry completion was a valid 58-byte
`0x74` report. The trackpad recorded 891 valid reports, 677 motion events,
four click presses and releases and zero malformed frames. Keyboard input
returned 22 valid reports. Both files verified, and offline allocation
passed for 128 files with zero writes.

The repeat stages used byte-identical kernel, initramfs and EFI loader
artifacts from `3388f68afd5dc712`. The release IDs gave each startup and
final report its own path. The keyboard worked in all three warm boots.
The desktop checklist passed in two boots. On both boots with an eight-byte
post-retry sample, its first occurrence preceded the first completed
keyboard request. The first rejected bytes matched exactly. The failure
therefore reproduced while the trackpad queue continued to complete USB
requests. The underlying reason the report format persisted is still open.
Ubuntu consumed all three one-shots, and all six flash reports matched
their archived copies on the final remount. Prior user files were preserved.

The desktop recovery now waits an interval of one second before retrying
again on an eight-byte `0x02` startup response, up to four total attempts.
It stops retrying after valid trackpad reports begin. The first response
after each attempt and the time to the first valid frame are recorded in
the startup and final diagnostics. The worker preserves both queues and
their data toggles. A failed feature transfer can resume the keyboard
schedule after idle and parked-descriptor checks. The existing report
path records these attempts and continues to count other malformed frames.
Linux uses the same one-second throttle for a repeated mode reset.
[Linux mode recovery](https://github.com/torvalds/linux/commit/fc1e8a6f129d87c64ac8e58b50d9dfa66217cfda)

Source `c1145332`, release `942314bc1c6ab8ed`, passed the EHCI, keyboard
and pointer fixtures and the exact-stage normal and keyboard-only VM
rehearsals. On the MacBook the first retry yielded a valid 58-byte report
after 7,203 µs. Motion, physical clicks, resource release and 21 valid
keyboard reports verified. The exact Find-field text marker was absent,
so the desktop aggregate was incomplete. This boot did not exercise an
additional retry. Both reports matched after remount and the 130-file
offline allocation check passed.

Release `66f988ee64dc5135` reproduced the failure on the same build. All
four retry cycles completed and read normal mode `8` before writing
Wellspring mode `1`. The trackpad returned 406 eight-byte responses and
zero valid frames. The keyboard-only exit worked. Both flash files and
the 132-file allocation check verified.

The follow-up desktop recovery resumes periodic polling between the normal
and Wellspring writes. It keeps both queues live for up to 300 ms in normal
mode. A `60 02` status response allows the switch after the minimum 1 ms
wait. The timeout also advances the switch when the trackpad is quiet.
Both control exchanges still acquire an idle periodic schedule and keep
pending descriptors and data toggles intact. Each attempt records normal
mode completions and the status response alongside the final report.

Source `35572c70`, release `be59d55498834452`, passed the EHCI, keyboard
and pointer fixtures, both exact-stage VM exits and the native desktop
checklist. The first eight-byte response started a normal-mode poll.
The status wait expired at 309,459 µs with zero normal-mode completions.
After the Wellspring write, the next observed completion was a valid
58-byte `0x74` report, 8,724 µs after resuming. The desktop verified the
Find text, both pointer axes, physical clicks and pointer exit. It recorded
993 valid trackpad reports, zero malformed frames and 22 valid keyboard
reports. Mode restoration and topcase retirement verified.

The startup and final reports matched their archives after Ubuntu returned
and the drive was remounted. The offline allocation check passed for 134
files with zero writes.

Three more warm boots used the same kernel, desktop image and EFI loader
as `be59d55498834452`. Each boot passed the full desktop checklist and the
strict native result. Each observed one initial eight-byte response and
returned a valid 58-byte `0x74` report on the first completion after one
recovery attempt. The normal-mode dwell was 309,466–309,474 µs and the
valid completion followed resumption by 8,748–8,769 µs. No normal-mode
status responses or malformed frames were observed.

| Release | Valid trackpad reports | Valid keyboard reports | Flash readback |
| --- | ---: | ---: | --- |
| `886c207a4111afe2` | 873 | 21 | Startup and final |
| `256f075ccad6f9a4` | 888 | 36 | Startup and final |
| `207ea10a0545a290` | 844 | 22 | Startup and final |

Ubuntu consumed all three one-shots. All six reports matched their archives
after final remount. The offline allocation check passed for 140 files with
zero writes. This is four consecutive native passes for the revised image.
The persistent eight-byte stream did not recur. Recovery from an already
persistent stream remains unverified on this build.

## Live desktop saved-file checkpoint

Runtime source `0c067180` retains the captured `00:1d.7` flash controller
while the desktop runs. The v5 file trial at staging source `1664abbe`
opens a new release-bound document at `/work/srvros-work`. The startup and
final reports use the same mounted workspace. The file trial verifies the
edited bytes after Text exits and flushes the reports before unmounting
and retiring the flash controller. Its on-screen checklist uses pointer
placement to reach the last line on the built-in keyboard layout.

Release `75525ae902f943ab` passed the EHCI, keyboard and pointer fixtures.
The exact-stage VM used disposable MBR/exFAT media and verified Text
save and reopen, both report files, clean allocation and Ubuntu default
return. The keyboard-only variant saved both reports and left its trial
document at the original content.

The MacBook mounted the flash workspace over the captured EHCI controller,
then Text opened the 542-byte seed, saved the edit and reopened the
549-byte document. The native 1280 × 800 desktop passed typing, both
pointer axes, maximize and restore, and return. The topcase recorded
1,143 valid trackpad reports, 26 valid keyboard reports and zero malformed
frames. Ubuntu consumed the one-shot. After remount, all three new files
matched their archives and the 140 previous user files were preserved.
Offline allocation passed for 143 files with zero writes.

## Capability ledger

| Capability | State | Next evidence |
| --- | --- | --- |
| Native boot and display | Console and the 1280 × 800 desktop verified on the internal panel | Retain desktop boot coverage during later trials |
| Keyboard | Built-in monitor commands and desktop typing verified, including integration release `7cf2eafd5fb1972a` | Retain keyboard coverage during further topcase work |
| Trackpad | Revised normal-mode polling passed motion, clicks and strict transport checks on four consecutive warm boots. Motion and clicks also passed on integration release `7cf2eafd5fb1972a` | Capture a persistent eight-byte stream if it recurs |
| USB workspace | Live desktop Text save and reopen and both report files verified, including on integration release `7cf2eafd5fb1972a` | Qualify longer desktop file activity in a separate trial |
| Brightness | Internal panel adjustment and restoration verified on `4befbb5325d5ed2f` | Retain the explicit brightness trial |
| Audio | Built-in right speaker and stream teardown verified on `024a87e1e8d4bf42`. This unit has a broken left speaker | Qualify audible left-channel output with working hardware |
| Fan and CPU temperature | Read-only internal fan, core and package readings verified on `5e4d9efffb95d338` | Retain firmware fan control |
| Battery and AC power | Connected, disconnected and reconnected readings verified on `0cb82183e1678a63` | Retain read-only charger and battery coverage |
| Wired Ethernet | The production BCM57765 provider passed connected and disconnected desktop startup on `657931e51d2152c8` and `e0c0bff8e7ef17ae`, including 1 Gb/s full-duplex DHCP, sustained gateway traffic, and three cable cycles | Retain both startup and cable recovery checks |
| Wi-Fi | Settings Connect, Disconnect, and reconnect passed on BCM4331 against `Demers2.4` in `d6defd5b7a97090b`. The original association resumed gateway traffic after a 15-second driver receive gap in `c809111f83871175`. Both trials saved both reports | Access-point outage and automatic reassociation testing deferred. Investigate the startup refusal if it recurs |

The local host checkpoint, exact VM evidence and first console return are in
`build/review/`. Keyboard reports and trackpad input follow the native
enumeration checkpoint. Keep the verified console payload available for
recovery and retain the existing xHCI regression coverage.
