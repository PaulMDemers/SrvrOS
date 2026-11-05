# Native strict entropy

`SYS_RANDOM` (92), through the existing `srv_random(buffer, length, flags)` SDK
forwarder, accepts `SRV_RANDOM_REQUIRE_ENTROPY` from `<srvros/random.h>`. This
opt-in mode consumes only existing virtio-RNG pool words or successful RDRAND
instructions. It never uses the legacy clock/PID/software generator. Provider
access retains the existing CPU0 interrupt exclusion; RDRAND has at most ten
attempts per word. There is no wait, allocation, new worker or device activation.

The maximum request remains 256 KiB. Unsupported flag combinations, oversized
requests and invalid writable user ranges return the historical generic `-1`
before provider consumption. A zero-length request succeeds without a provider.
Success returns the exact byte count written, possibly a positive short prefix
when the providers exhaust. `SRV_RANDOM_UNAVAILABLE` aliases `SRV_ERR_AGAIN`
(-11) and means no bytes were written because no provider word was available.
A later user-copy failure returns -1 and can follow earlier committed chunks;
callers must treat any negative result as failure. No unwritten suffix counts as
success. Zero-valued provider words are valid; byte statistics are not a test of
provider quality.

Flags0 and POSIX `getrandom` keep their previous best-effort behavior, including
the software fallback and the current POSIX wrapper's flag handling. They do not
provide this strict contract. An older kernel rejects the new flag with -1;
cryptographic callers must fail closed and must not retry with flags0.

PR334's WTF adapter requests at most256 bytes per native call, advances on
positive short reads, and aborts on negative, zero, or oversized results. Its
constructor/destructor do no file I/O. The port's generated ABI copy and mandatory
build parity check are documented in [the port record](../ports/srvros/webkit/PORT.md).
The retained qualified SDK remains unchanged; new SDK exports include the native
header. This does not change any structure or syscall signature.

The qualified emulated positive profile adds only RDRAND to `qemu64`; the absent
provider control retains plain `qemu64`, with no virtio-RNG device. QEMU11.1.2
[emulates RDRAND](https://github.com/qemu/qemu/blob/v11.1.2/target/i386/tcg/int_helper.c)
using its [guest random source](https://github.com/qemu/qemu/blob/v11.1.2/util/guest-random.c).
Qualifying configurations must exclude deterministic `-seed` and replay modes.
This is bounded emulator provenance, not physical hardware entropy acceptance.

Four reviewed guests at source6b5ebaf7/Ports d99565f pass absent/positive native
contracts, the native context regression, and the eight-case experimental WTF/GC/
JavaScript smoke. See [the checkpoint](browser-port.md). Quota fallback messages
remain; no resource limit was changed. These are emulator/contract observations.
