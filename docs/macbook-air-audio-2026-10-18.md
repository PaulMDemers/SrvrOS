# MacBook Air audio output bring-up

Hardware target: MacBookAir7,2, Intel Wildcat Point-LP HDA controller
`8086:9ca0`, Cirrus Logic CS4208 codec `1013:4208`, codec subsystem
`106b:7e00`. Ubuntu's read-only codec dump is retained locally at
`build/hardware/cs4208-linux-codec.txt`. The separate `8086:160c` display-audio
controller is deliberately outside this initial driver.

## Scope and architecture

Boot option `srvros.audio=hda` opts into controller reset, codec discovery and
output setup. Normal boots without it do not touch the controller. Discovery
matches the exact supported controller and codec profiles. No tone is played
on initialization; codec output amplifiers remain muted while idle.

`hda_core.c` has width-aware register callbacks and bounded reset, immediate
codec commands and output-DMA operations. CORB/RIRB DMA and controller interrupts
are disabled; immediate-command transport is serialized by one audio worker.
The driver does not silently reuse a command channel after an uncertain reply.
Register waits have both a monotonic deadline and a finite poll budget. A stalled
command path yields periodically to other threads rather than blocking input for
its entire timeout. The transport is independent of PCI and the allocator, so
fault handling is exercised by the host model.

`hda_codec.c` validates a direct output pin-to-DAC path, stereo PCM support and
48 kHz/16-bit capability before setting the route. Initial profiles are QEMU's
mixed HDA output/duplex codecs and the captured Apple CS4208 subsystem. The Apple
profile uses speaker pin `0x12` / DAC `0x03`, headphone pin `0x10` / DAC `0x02`,
and GPIO0 for the speaker amplifier. Other GPIO bits are preserved. The CS4208
A1 processing coefficients (`0x33=1`, `0x34=0x1c01`) are gated by this profile.
No microphone, HDMI, undocumented arbitrary codec-command interface or guessed
route is exposed.

`audio.c` owns a retained DMA buffer and two-entry buffer descriptor list. The
first output stream is derived from GCAP, with stream tag 1. A requested test is
500 Hz, stereo, half a second, with short attack/release ramps. Initial volume
is 25%; peak sample amplitude is 1200 of 32767. The remainder of the four-second
cyclic buffer is silence. The worker verifies stream progress and stops after
750 ms. Stop clears the buffer, stops DMA and mutes the codec. DMA memory is never
freed after an uncertain stop. Audio is an I/O-service worker: it never accesses
VFS or waits on an application, and remains schedulable during USB storage work.

The first API exposes a versioned cached snapshot and queued test/stop/volume/
output controls. Queries perform no hardware I/O. Mutating syscalls are limited
to the registered compositor; desktop protocol v11 permits changes from focused
Settings only. Replies distinguish accepting a request from its subsequent state.
Settings keeps a single request in flight, coalesces volume changes, queues Stop,
rejects stale responses and does not cancel slider capture on acknowledgement.
Volume and output selection are session-only.

## Physical acceptance checkpoint (2026-10-18)

The user confirmed audible volume changes in native release
`b427b78f21644bd2`. Saved logs show three test tones started/completed, volume
ending at 7 and no audio error. Basic speaker output/volume is accepted for this
Air; channel balance, PCM file playback, headphone routing and sustained native
playback still need specific confirmation. Other Mac models remain untested.

QEMU output validates sample production and routing through its own codec model;
it does not prove the MacBook speaker amplifier sequence, headphone route, volume
or audible quality. The persistent laptop Help checklist asks for repeated low-
volume speaker tests, both channels, silence after playback, volume changes,
input responsiveness, and optional wired-headphone checks. Output selection is
manual for this first round. Do not infer a passing physical test from enumerating
the codec or advancing the DMA position.

The earlier VM-only block is complete. The failed v21 boot stalled in optional
AML probing before the desktop; v22 omits that probe and reached this listening
round. See the [machine record](machines/macbookair7-2.md).

## References

The register and verb implementation follows Intel's
[High Definition Audio specification 1.0a](https://www.intel.com/content/dam/www/public/us/en/documents/product-specifications/high-definition-audio-specification.pdf).
The observed codec topology is from the target's Linux ALSA codec dump. The
model-specific GPIO/coefficient values were checked against the pinned Linux
[CS420x driver](https://raw.githubusercontent.com/torvalds/linux/v6.12/sound/pci/hda/patch_cirrus.c).
QEMU's pinned [HDA controller](https://raw.githubusercontent.com/qemu/qemu/v9.2.0/hw/audio/intel-hda.c)
and [codec](https://raw.githubusercontent.com/qemu/qemu/v9.2.0/hw/audio/hda-codec.c)
were consulted to verify emulator behavior. These source files are references;
no GPL implementation was copied into the driver.

## Reproduction

```sh
tools/host-env.sh make -j2 all test-core
tools/host-env.sh python3 tools/audio_smoke.py
tools/host-env.sh python3 tools/audio_smoke.py --controller ich9-intel-hda --codec hda-duplex
tools/host-env.sh python3 tools/audio_smoke.py --controller ich9-intel-hda --codec hda-duplex --soak-seconds 2400
tools/host-env.sh python3 tools/audio_settings_smoke.py
tools/host-env.sh python3 tools/audio_settings_smoke.py --usb-io
```

VM tests use disposable disks and the WAV audio backend, never the host speakers.
Evidence is saved under `build/review/audio-*`. The monitor `audio` command prints
state, codec identity, output, volume, command/stream diagnostics and test counts;
`audio test`, `audio stop`, `audio speakers`, and `audio headphones` queue actions.

Host tests exercise both the register/codec model and the production service
worker: request ownership, Stop priority, stereo/ramp/silent-tail synthesis,
zero volume, playback/stop/jack failures and 1,000 buffer-reuse cycles. The soak
checks repeated complete tones and USB keyboard input. WAV analysis streams the
capture, verifies each tone's duration, frequency, stereo samples and quiet tail,
and saves a machine-readable result. The GUI test covers asynchronous Test/Stop,
volume changes and clean desktop exit. These checks do not replace listening to
the physical outputs.

The USB-I/O variant uses a freshly formatted disposable exFAT volume and a
bandwidth-limited writer. Both test tones must occur before the writer finishes;
the harness verifies the full 1 MiB output, guest unmount and host read-only fsck.
Captured audio must contain two complete stereo tones at the requested gains.

## Sustained PCM output and WAV player

The evening follow-up adds `SYS_AUDIO_STREAM` (130), a bounded, exclusive PCM
output lease owned by the calling process. The format is 48 kHz, signed
little-endian 16-bit stereo. Open returns a nonzero generation handle; writes
accept at most 16 KiB and return the accepted byte count or a retry result.
Finish seals and drains the stream; Close or process cleanup requests silence
and releases ownership. Another process cannot query, write, finish or close
that lease. The compositor's existing Stop control silences DMA and revokes the
lease without waiting for the old process to finish filesystem cleanup. Stale
handles cannot affect a replacement stream.

The same retained four-second HDA DMA allocation serves tones and files. There
is no per-write allocation and no userspace DMA pointer. The producer copies
only into available ring space, with a half-second guard covering the maximum
16-bit FIFO size and the permitted age of a cached playback position. A normal
stream starts after one second is queued; Finish starts shorter files directly.
The worker clears consumed bytes to silence, counts starvation/gap insertions,
and stops on stalled, implausible or ambiguous position movement. It accepts the
specification's inclusive end-of-buffer position and clears complete frames only.
A service gap of one second is a fault, before a four-second wrap can become
ambiguous. As before, uncertain DMA memory is retained and silenced.

Settings disables volume/output/Test while a file owns sound; Stop remains
available. Gain and route are chosen before playback. This is deliberately a
single-stream implementation: no mixing, seeking, pause or microphone
capture is provided. Cached stream state, ownership, submitted/queued/position
bytes and underrun counts are included in periodic and exit diagnostics.

`playwav FILE.wav` checks the RIFF/chunk bounds and exact PCM format before
opening audio. It accepts 16-bit PCM at 44.1 or 48 kHz in mono or stereo.
A bounded userspace converter duplicates mono channels and uses integer linear
interpolation for 44.1 kHz sources; the kernel format remains fixed. Compressed,
extensible and other bit-depth/rate formats are rejected. It streams bounded
reads, handles backpressure, drains the final
samples and reports underruns. Malformed, incomplete and unsupported files are
rejected. Ctrl-C or process termination uses the same kernel ownership cleanup.
The bundled `/fat/share/examples/stereo.wav` lasts six seconds, with left-only,
right-only and distinct stereo sections separated by silence. From Terminal:

```sh
playwav /fat/share/examples/stereo.wav
```

Set a low output volume in Settings first. Type in Text or move a window during
playback; Settings Stop should cancel it. Physical audible quality and channel
routing still require the laptop round.

Validation commands:

```sh
tools/host-env.sh make test-audio-stream test-wav test-pcm-convert test-settings
tools/host-env.sh python3 tools/audio_stream_smoke.py
tools/host-env.sh python3 tools/audio_stream_smoke.py --lifecycle
tools/host-env.sh python3 tools/audio_stream_smoke.py --mono-44100
tools/host-env.sh python3 tools/audio_stream_smoke.py --format stereo-44100
tools/host-env.sh python3 tools/audio_stream_smoke.py --format mono-48000
tools/host-env.sh python3 tools/audio_desktop_stream_smoke.py
tools/host-env.sh python3 tools/audio_stream_smoke.py --seconds 60 --repeat 10
```

The waveform checker streams the capture and compares both channels against the
source, including across DMA-ring wraps. Lifecycle coverage checks foreign-owner
syscalls, competing players, keyboard input during background playback, killed
players and subsequent reuse. It also rejects truncated, unsupported-rate and
missing files before confirming that valid playback still works. Provenance
records the actual ISO, copied boot kernel/initramfs/configuration and initial
guest filesystem hashes, plus source revision and dirty-worktree state. A stale
boot-tree kernel fails before the VM starts. These results are recorded separately from the
earlier test-tone soak and do not establish physical sound acceptance.

### Playback during storage work

A USB-write overlap test exposed unnecessary scheduler gating of already-open,
read-only file snapshots. `SYS_READ` now admits explicit descriptors >=3 for at
most 16 KiB when their type is `PROCESS_FILE_VFS`. That path only copies from a
retained memory snapshot and advances the shared cursor under IRQ exclusion.
Opening, closing, writable-file reads, redirected stdio, larger reads and all
filesystem mutations keep their existing ownership gate. This is not concurrent
block-device access. The WAV player's file-close runs after audio release. Its refill loop uses
raw bounded snapshot reads and tick sleeps: general stdio/POSIX wrappers query
thread/signal state through other gated syscalls. Default termination is still
handled at each syscall boundary.

The existing filesystem loads read-only file contents when opening them; bounded
player buffers do not remove that underlying whole-file snapshot limit. Future
large-media support needs paged/streaming filesystem reads and asynchronous I/O.

### Emulator capture distinction

The initial ten-minute PCM run completed ten files with zero guest underruns,
but its sixth captured waveform lost exactly 2,048 frames. It is retained as a
failed waveform check, not a passing soak. A later captured run reproduced a
waveform discontinuity with QEMU's `hda_audio_overrun` trace. QEMU 10.2.1's
[timer-backed codec](https://raw.githubusercontent.com/qemu/qemu/v10.2.1/hw/audio/hda-codec.c)
discards its 8,192-byte buffer when the output callback finds it full; at this
format that is 2,048 frames. This can occur under competing host VM load without
an underrun in the guest DMA ring.

The stream harness now records this trace and defaults to `--codec-timer off` for
sample-exact capture through QEMU's callback-driven codec. `--codec-timer on`
selects the timer codec for separate diagnostic runs. It still fails any
waveform mismatch; neither backend errors nor guest underruns are silently
ignored. The staged boot harness also defaults to callback mode for reproducible capture;
`--audio-codec-timer on` selects QEMU's timer codec with overrun tracing.
These tests establish separate emulator and driver observations, not physical
audio acceptance.
