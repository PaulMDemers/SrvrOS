# GLib 2.90.1 prerequisites on SrvrOS

November 22, 2026. The original target configure stopped at GLib's
`sys/poll.h` lookup. A diagnostic copy using the SDK's `<poll.h>` reached
the missing `iconv` dependency. The follow-up adds a SrvrOS poll fallback
and a target GNU libiconv provider. The next target dependency is PCRE2
8-bit.

The tested SDK and normal kernel came from audit branch `14edc87b` after
merging OS main `b275e57d`. The matched ports pin was `276eb1b0`, apps
`73e55463`, and SrvrUI `3a695ce5`. The [WebCore audit](browser-webcore-audit.md)
retains its original source attribution.

## Source pin and dependencies

Pin GLib **2.90.1** for the SrvrOS port investigation. The official tarball is
5,871,112 bytes with SHA-256
`93c941aa17d5eb1d53fe838365f29a8b4e539c222a256d974ec8f30fc413e396`.
The release contains the LGPL 2.1 license text in `COPYING`. The probe uses
Meson 1.10.2. GLib's stated Meson minimum is 1.4.0.
[GNOME release archive](https://download.gnome.org/sources/glib/2.90/glib-2.90.1.tar.xz)
and [official checksums](https://download.gnome.org/sources/glib/2.90/glib-2.90.1.sha256sum).

| GLib input | Observed or upstream requirement |
| --- | --- |
| POSIX threads and atomics | The diagnostic configure links its thread and atomic checks against the SDK. |
| `iconv` | Mandatory for this target. Meson detects the static GNU libiconv 1.19 provider from an isolated target prefix. |
| PCRE2 8-bit | Minimum 10.32. Upstream wrap specifies 10.46 plus a hashed Meson wrapper. |
| GVDB | Upstream subproject pinned to `2b42fc75f09dbe1cd1057580b5782b08f2dcb400`. |
| libffi | Minimum 3.0.0. Upstream wrap specifies 3.5.2 plus a hashed Meson wrapper. |
| zlib | Source exists at the OS ports pin. GLib target package/export still needs a real link check. |
| `intl` | GLib tries libc/system gettext and then its proxy-libintl wrap. `-Dnls=disabled` alone does not remove this lookup. |

The original header diagnostic reached `iconv`. The follow-up reached
PCRE2 8-bit, which Meson could find neither in the target packages nor as a
local subproject. The remaining rows describe the pinned source's graph.
The target profile uses an empty pkg-config directory and disables Meson
wrap downloads.
[Pinned Meson source](https://github.com/GNOME/glib/blob/2.90.1/meson.build).

## Target poll and iconv

The [poll patch](../tools/glib-srvros-poll.patch) preserves the upstream
`sys/poll.h` test and its public feature macro. For a SrvrOS target with
`sys/types.h` and `poll.h`, it reads all six poll constants from the SDK.
The static cross-configure uses the SrvrOS CRT and libc for executable probes.

The build pins GNU libiconv **1.19**, 5,921,103 bytes with SHA-256
`88dd96a8c0464eca144fc791ae60cd31cd8ee78321e67397e25fc095c4a19aa6`.
The GNU detached signature verified against the project's published
keyring, using fingerprint `E0FFBD975397F77A32AB76ECB6301D9E1BBEAC08`.
The recipe builds upstream libcharset and the static libiconv archive using
the SrvrOS compiler wrapper, then stages the public header, archive and
`COPYING.LIB`. The library is LGPL 2.1 or later. The SDK exposes a C locale.
The build disables the locale-backed `WCHAR_T` fallback because the SDK's
`mbrtowc` and `wcrtomb` do not provide the multibyte behavior it requires.
[GNU source archive](https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.19.tar.gz)
and [detached signature](https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.19.tar.gz.sig).

With the two checked archives and Meson 1.10.2 available, run the recipe
from the SrvrOS root. It prints a fresh run directory and exits with
Meson's status.

```sh
python3 tools/browser_glib_iconv.py --iconv-archive /path/to/libiconv-1.19.tar.gz \
  --glib-archive /path/to/glib-2.90.1.tar.xz --meson /path/to/meson
python3 tools/glib_platform_guest.py --iconv-prefix /path/to/run/target-iconv
```

## Verification

- **Original target configure.** Meson used `system='srvros'`, x86_64, Zig 0.15.2, the
  existing SrvrOS C wrapper and SDK, and an empty target pkg-config directory.
  Target executable probes used static links. Stock Meson linked `eventfd`,
  `ppoll`, `clock_gettime`, and `dlopen`/`dlsym` before `FIX POLL* defines`.
  The `<poll.h>` diagnostic changed only the lookup and corresponding include
  in a disposable source copy. Meson resolved all six poll constants, linked
  its thread checks, and stopped at the mandatory `iconv` lookup.
- **Follow-up target configure.** Meson found `<poll.h>`, computed all six
  constants and found target `iconv` using the staged header and archive.
  The first unresolved dependency was `libpcre2-8`. Meson exited at that
  dependency before any GLib library build.
- **Normal SrvrOS guest.** The native fixture links strictly against the same
  SDK. Both runs passed in one diskless, no-NIC, one-CPU Q35/TCG guest. The
  fixture verifies nonblocking close-on-exec pipes, two pthread wakes, draining
  to `EAGAIN`, a measured quiet timeout, fd retirement, and synthetic eventfd
  readiness and counter drain. It checks socket/eventfd refusal by native GUI
  wait and confirms `dlopen` returns `ENOSYS` despite linking. The existing
  GUI peer fixture separately verifies deadlines, delayed pipe wake, cancellation,
  descriptor replacement, simultaneous GUI/pipe readiness, and peer exit.
  Both probe executions returned exit 0 and the monitor prompt. QEMU stopped.
- **Target conversion.** The separate statically linked libiconv fixture
  passed twice alongside two reruns of the native wait fixture. It checked
  non-ASCII UTF-8/UTF-16LE in both directions, Latin-1, `E2BIG` and retry,
  `EILSEQ`, `EINVAL`, pointer progress, and ISO-2022-JP shift-state reset.
  It also checked the expected rejection of `WCHAR_T`. All four executions
  exited 0 and returned to the monitor, and the guest stopped. Relinking
  against the final recipe's archive produced exactly the booted runtime
  ELF bytes after the standard packaging step.
- **Host GLib.** Homebrew's GLib 2.90.1 passed an independent real
  `GMainContext` test using acquire, prepare, query, poll, check, and dispatch.
  One owner processed a worker-attached source and a timer. The quiet wait
  slept for about 83 ms and the wake/timer portion completed in two sleeping
  iterations. This verifies the algorithm on the host. Running actual GLib
  within the SrvrOS guest awaits target library bring-up.
  [GLib main-loop API](https://docs.gtk.org/glib/main-loop.html).

The [original receipt](assets/evidence/glib-prereq/receipt.json) and
[poll and iconv receipt](assets/evidence/glib-iconv/receipt.json) record
configuration outcomes, matched inputs and result hashes. The first strict
one-step Zig compile/link rejected an unused driver option. Separate strict
compilation and the normal SDK linker invocation resolved it. The test and
runner have no dependency on files from the earlier WebCore extraction.

## Wait integration recommendation

The pinned GLib wakeup code prefers `eventfd` when available and falls back to
nonblocking pipes. SrvrOS's synthetic eventfd links and works through libc poll,
but native GUI wait captures only nonblocking pipe/PTY interests with supported
events, at most eight descriptors and a one-second deadline horizon. Native
GUI wait has no access to the synthetic eventfd counter. A successful link also
does not prove the upstream async-signal-safe wake contract. SrvrOS's synthetic
eventfd path uses a libc mutex. Require signal-context evidence before relying
on that backend for a signal-originated wake.
[Pinned GLib wakeup source](https://github.com/GNOME/glib/blob/2.90.1/glib/gwakeup.c).

Keep the read and dispatch operations on one owner. A future adapter can call
GLib prepare/query, preserve the complete interest list, and use native GUI wait
only when every interest is an admissible pipe or PTY. Clamp that wait to the
earliest GLib timer or native deadline. Treat cancellation, close and descriptor
replacement as a fresh query. For mixed or unknown interests, do a nonblocking
GUI service turn, poll the complete GLib set with a bounded sleep capped by the
earliest timer, and then check/dispatch using the queried array. A 10 ms sleep
cap is a starting value for latency measurements. A due source gets immediate
service, and quiet iterations must sleep. Unknown descriptors remain in libc
poll so readiness and errors reach the source. The boundary proven here is the
host GLib loop plus the guest's underlying wait primitives. An integrated guest
test must still measure wake latency, verify actual GLib backend selection,
exercise mixed interests and show idle progress without a busy loop.

The current WPE IPC gate remains concrete. Its GLib launcher requests
`SOCK_SEQPACKET`. SrvrOS `socketpair` accepts only AF_UNIX/SOCK_STREAM. Native
stream descriptor transfer is bounded to four descriptors while WPE's receiver
allows up to 254 attachments. The runtime owner should establish the actual
requirements of the first page and its handoffs before defining a transport.
SrvrOS `mmap` also rejects `MAP_SHARED`. UI frame release versus publication
continues to require explicit outcome and cancellation design.

## Reproduction

From the SrvrOS root, build the matched SDK and normal guest inputs, then run
the fixture. Use a disposable output directory for configure retries.

```sh
sh tools/macos/build.sh -j2 srvros-sysroot build/kernel/srvros.elf \
  build/userspace/init.elf build/limine-binary/.ready
python3 tools/glib_platform_guest.py

clang -std=c11 -O2 -Wall -Wextra -Werror $(pkg-config --cflags glib-2.0) \
  tools/test_glib_main_context_host.c $(pkg-config --libs glib-2.0) \
  -o build/browser-glib-prereq/glib-main-context-host
build/browser-glib-prereq/glib-main-context-host
```

Verify the archive against the pin above and extract it under
`build/browser-glib-prereq`. Configure Meson with this cross file, replacing
`<SrvrOS>` with the absolute checkout path and `<pkg-config>` with its absolute
host tool path. Create the empty pkg-config directory first.

```ini
[binaries]
c = '<SrvrOS>/ports/srvros/webkit/cc'
ar = ['<SrvrOS>/build/tooling/zig/zig', 'ar']
pkg-config = '<pkg-config>'

[host_machine]
system = 'srvros'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
needs_exe_wrapper = true
pkg_config_libdir = ['<SrvrOS>/build/browser-glib-prereq/empty-pc']
```

```sh
cd build/browser-glib-prereq
env -u PKG_CONFIG_PATH -u CFLAGS -u CPPFLAGS -u LDFLAGS -u CPATH -u SDKROOT \
  uvx --from meson==1.10.2 meson setup target-config glib-2.90.1 \
  --cross-file srvros.ini --wrap-mode=nodownload --default-library=static \
  -Dtests=false -Dintrospection=disabled -Dnls=disabled -Dlibmount=disabled \
  -Dselinux=disabled -Dxattr=false -Dsysprof=disabled
```

For the diagnostic, copy that source and replace `sys/poll.h` with `poll.h`
only in the `has_syspoll` check and the adjacent constants-test include in
`meson.build`. Use that copy and a new output directory with the same flags.

The next concrete dependency task is a pinned target PCRE2 8-bit build
and a fresh isolated GLib configure against it.
