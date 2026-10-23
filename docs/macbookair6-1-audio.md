# MacBookAir6,1 audio listening trial

This unit's captured analog controller is Intel `8086:9c20`, with CS4208
`1013:4208`, subsystem `106b:7100`, revision `100300`, AFG 1. The separate
`8086:0a0c` HDMI controller is not admitted. Native audio acceptance is pending.

## Route evidence and bounds

The local October 22 Ubuntu codec dump records speaker pin `12` connected only
to DAC `03`, raw default `90170011`; headphone pin `10` connected only to DAC
`02`, raw default `032b401f`. Both DACs advertise stereo 48 kHz/16-bit PCM.
Speaker pin has no amplifier; DAC amplifiers support mute and unity offset 127.
Headphone pin has a separate mute amplifier with unity offset 66. GPIO0 is
configured as an enabled output, high with speakers selected. Pins `11`, `13`
and `14` have raw speaker defaults but Linux leaves their outputs disabled;
SrvrOS does not enable them or the unused DACs.

The pinned [Linux v6.12 CS4208 reference](https://raw.githubusercontent.com/torvalds/linux/v6.12/sound/pci/hda/patch_cirrus.c)
independently maps this exact subsystem to its MBA6 pin correction: speaker
pin `12`, headphone pin `10`, GPIO0 speaker enable. Its common CS4208 A1
processing setup uses coefficients `33=1` and `34=1c01`. This corroborates reuse
of SrvrOS's existing coefficient/GPIO mechanism; no Linux implementation was
copied. SrvrOS checks the captured raw defaults, not Linux's software overrides.
A fresh read of this unit's `init_pin_configs` additionally records firmware
values `90100110` / `002b4020` for those same routes. Discovery accepts either
complete observed pair, refusing mixtures; the raw live dump still matches the
original inventory. Both raw files are retained with the local trial evidence.
Air6,2 and other subsystem IDs remain refused by this profile.

Revision, both raw pin defaults, direct connections, widget types and PCM
capabilities are checked before codec mutations. Controller reset and snooped
DMA setup precede discovery as on the existing PCH profiles. Initialization
selects speakers muted and does not play sound. The existing worker supplies
bounded Test/Stop, volume, manual output selection and PCM streaming. Other
GPIO bits are preserved. No microphone, HDMI or automatic jack switching is added.

## Exact stage and validation

```sh
tools/host-env.sh make -j2 all usb-image test-core test-desktop
python3 tools/test_hardware_boot.py
python3 tools/hardware_boot.py stage --desktop-volume 6ADA-85BB --desktop-basic --desktop-brightness --desktop-audio
tools/host-env.sh python3 tools/hardware_boot_smoke.py --desktop-check --audio-check --workspace-table gpt
```

The distinct v57 recipe keeps native unscaled desktop/input, accepted brightness
and opt-in audio. Battery/AML, SMC, sensors and networking remain off. Existing
v53/v56 recipes retain their exact configurations; v55 belongs to the reference
Air's separate combined-services work. The HDA host model checks muted startup,
speaker/headphone transitions, unrelated GPIO preservation, unused outputs,
revision/pin/connection/format refusals and sibling mismatch before codec writes.
The exact-stage VM checks desktop save/reopen/preferences and captured tone/WAV
samples, silence and DMA completion. QEMU cannot prove native sound quality.

## Attended checklist

1. Open Settings. Audio should be ready with speakers selected and no unsolicited
   sound. Start at 10% volume; press Test once, then repeat at 25%. Note whether
   the short tone is clear and changes volume; do not raise volume to compensate
   for silence or distortion.
2. In Terminal run `playwav /fat/share/examples/stereo.wav`. Listen for left-only,
   right-only and stereo sections with silence between. Move a window and type
   during playback. Replay and press Settings Stop; sound should stop and remain
   silent afterward. Repeat Test to check recovery after Stop.
3. Optionally plug in wired headphones, lower volume, choose Headphones manually
   and repeat Test/WAV. Check that speakers are silent. Stop before unplugging,
   select Speakers and retest. Report if headphones were not tested.
4. Close apps, use the desktop exit to finish saved diagnostics, then `reboot`
   from the `srv>` monitor to return to Ubuntu. Report sound quality, both
   channels, volume changes, Stop and any unwanted pop/buzz or input trouble.

Retain the accepted brightness desktop and console recovery payloads. Install
with `--keep-previous`; Ubuntu remains default and reboot stays unarmed until
requested. On return collect the matching release/session, compare existing USB
files, and check offline exFAT allocation before recording physical acceptance.
