# MacBookAir6,1 audio listening trial

This unit's captured analog controller is Intel `8086:9c20`, with CS4208
`1013:4208`, subsystem `106b:7100`, revision `100300`, AFG 1. The separate
`8086:0a0c` HDMI controller is not admitted. Basic native speaker playback is accepted; headphone routing remains pending.

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

## Native result, 2026-10-23

Release `0533c0dfc63d104f` / compiled `8f3656a` returned two clean desktop
sessions. The user reports proper sound. The listening session completed three
tones, a full 1,152,000-byte WAV and cancellation of its replay, ending ready
with zero underruns/errors and no PCM owner. Volume requests include 10% and 25%.
Basic speaker playback is accepted. Detailed channel balance, headphones,
startup-pop behavior and sustained output remain unconfirmed. See the
[machine record](machines/macbookair6-1.md) for session IDs and USB checks.

## Next trial: headphones and five one-minute playbacks

Use the accepted v57 payload `0533c0dfc63d104f`, compiled `8f3656a`. This is
a new physical run of the same payload, not acceptance of newer mainline code.
The local host generates `stereo-60s.wav` with the existing
`tools/audio_stream_smoke.py` fixture: 48 kHz signed 16-bit stereo, distinct
500/750 Hz channels, ramped ends and a quiet tail. Five full plays exercise
five minutes of PCM and repeated ownership release across many DMA ring wraps;
the short pauses between plays are deliberate, not a continuous five-minute stream.
The prepared file is in Home's `audio-extended` directory on this unit's USB.

1. Keep wired headphones ready. In Settings select Speakers and 10% volume.
   Run the bundled channel test in Terminal:
   `playwav /fat/share/examples/stereo.wav`.
   Check left-only, right-only and stereo sections.
2. With playback stopped, plug in headphones. Check the jack indication,
   explicitly select Headphones and keep volume at 10%. Run the channel test
   again. Check left/right sound and that the laptop speakers stay silent.
   Selection is manual; plugging in does not promise automatic routing.
3. Run `playwav /work/srvros-work/audio-extended/stereo-60s.wav` five times,
   letting each finish before repeating. Expect each to report
   `complete bytes=11520000 underruns=0`. Count the completed runs. Keep the
   headphone route selected; move windows and type in Text during playback.
   Report any dropout, distortion, swapped channel or input stall. Brief silence
   at the end of each file is part of the fixture. Five completed plays are the
   target; a canceled or failed play does not count as completed.
4. Start a sixth play and use Settings Stop while it is running. Check prompt
   silence and return of control. Press Test to check playback recovers. Stop
   before unplugging, check disconnected indication, explicitly select Speakers,
   and repeat the bundled channel test. Confirm sound returns to the speakers.
5. Exit the desktop normally, wait for USB unmount, then type `reboot` at `srv>`.
   Report headphones left/right/isolation, completed one-minute play count,
   Stop/recovery, return to speakers, and any audible or input problem.

If a test fails, stop and report what happened; do not raise volume to compensate
for silence. A completed player log proves data flow, not audible correctness.
This trial does not claim automatic jack switching, microphone/HDMI support,
uninterrupted long-form playback or simultaneous USB write stress.

Host rehearsal uses the accepted kernel/initramfs hashes (not a rebuild of the
newer checkout), the existing HDA/service/stream/format/Settings tests and
`audio_stream_smoke.py --seconds 60 --repeat 5`. The soak's recorded checkout
revision describes its harness; payload identity is established separately by
matching guest kernel/initramfs hashes to the accepted manifest. The exact v57
GPT desktop/audio boot rehearsal remains a separate receipt. QEMU cannot prove
headphone wiring or isolation; those require the attended checks above.
