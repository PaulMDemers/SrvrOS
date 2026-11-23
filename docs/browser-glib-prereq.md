# GLib 2.90.1 prerequisites on SrvrOS

November 22, 2026. The original target configure stopped at GLib's
`sys/poll.h` lookup. A diagnostic copy using the SDK's `<poll.h>` reached
the missing `iconv` dependency. GLib now configures through the SrvrOS
poll fallback, GNU libiconv, PCRE2 8-bit, bundled GVDB, libffi, zlib,
proxy-libintl and GIO's `C_IN` and `res_query()` checks. Meson generates
the static build graph. GLib, GObject and GModule build as target static
archives. An executable linked against target GLib passed the child-watch
probe twice on SrvrOS. GIO's XDG MIME objects compile with SDK `fnmatch()`.
A local-file-only GIO link verified the polling fallback on private exFAT.
Guest-verified SDK address declarations let `ginetaddress.c` compile.
Linked guests verified the IPv4 ToS and IPv6 traffic-class codecs.
The linked resolver passed its IPv4 literal checks. Static GIO next stops
in `gsocket.c` at `SOCK_SEQPACKET` and other missing socket declarations.

The tested SDK and normal kernel came from audit branch `14edc87b` after
merging OS main `b275e57d`. The matched ports pin was `276eb1b0`, apps
`73e55463`, and SrvrUI `3a695ce5`. The [WebCore audit](browser-webcore-audit.md)
retains its original source attribution.
The PCRE2 run starts from main `c477c26e` (PR #379) and reuses the same
SDK and guest boot inputs, verified by hash.
The libffi run starts from the PCRE2 topic at `9172c3d9` with those same
SDK and guest boot inputs.
The zlib run starts from main `7373d4d5` (PR #505), with those boot inputs
and SDK verified again by hash.
The combined run uses the rebased zlib commit `5d3d49be` on main
`288886bd`, with the same verified SDK and boot bytes.

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
| PCRE2 8-bit | Minimum 10.32. The SrvrOS build uses 10.49, and Meson finds it through the target-only pkg-config directory. |
| GVDB | The pinned GLib release already contains the subproject, and Meson configures it offline. The wrap names `2b42fc75f09dbe1cd1057580b5782b08f2dcb400`. |
| libffi | Minimum 3.0.0. Meson finds the target static 3.8.0 provider. |
| zlib | Meson finds the browser target's static 1.4 provider. |
| `intl` | GLib configures the pinned proxy-libintl 0.5 wrap as an internal dependency, with `-Dnls=disabled`. |
| GIO resolver headers | The SDK exports `C_IN` in `<arpa/nameser.h>`. GIO's primary compile check passes. |
| GIO resolver link | Meson links `res_query()` directly from the SrvrOS SDK using `<resolv.h>`. |
| GIO resolver object | `gthreadedresolver.c` compiles with `dn_expand()`, wire cursors, the fixed DNS header and the record constants. |

The target profile searches only the staged PCRE2, libffi and zlib pkg-config
directories and disables Meson wrap downloads. The `intl` lookup uses the
verified local wrap source. The optional standalone `m` lookup returns absent,
and the atomic link check passes.
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

## Target PCRE2

The port pins PCRE2 **10.49** from the upstream release, with SHA-256
`53c156e1ba416a20da8e65395daa132da0d80e76910424caca3fcdae7831d384`.
The build uses only the static 8-bit library with Unicode enabled and JIT
disabled. The target prefix contains the archive, generated header and
pkg-config metadata, and upstream `LICENCE.md` (BSD-3-Clause with the PCRE2
exception). Meson searches only that package directory for target packages
and keeps wrap downloads disabled.
[PCRE2 10.49 release](https://github.com/PCRE2Project/pcre2/releases/tag/pcre2-10.49).

```sh
python3 tools/browser_glib_iconv.py --iconv-archive /path/to/libiconv-1.19.tar.gz \
  --glib-archive /path/to/glib-2.90.1.tar.xz \
  --pcre2-archive /path/to/pcre2-10.49.tar.bz2 --meson /path/to/meson
python3 tools/glib_platform_guest.py --iconv-prefix /path/to/run/target-iconv \
  --pcre2-prefix /path/to/run/target-pcre2
```

## Target libffi

The static x86-64 port pins **libffi 3.8.0** with SHA-256
`7da3e2d9a171eb0a038f592ecad3ff2bb2550f3496d87b3b29ad0cf4430c0db4`.
The target prefix contains the generated public headers, archive, pkg-config
metadata and upstream license. GObject's generic marshaller uses direct FFI
calls. GIRepository also needs callable closures even with GIR generation
disabled.
[libffi 3.8.0 release](https://github.com/libffi/libffi/releases/tag/v3.8.0).

The [SrvrOS patch](../tools/libffi-srvros-closures.patch) reuses libffi's
x86-64 calling conventions and closure entry points. It allocates up to 128
simultaneous closures per process. Fixed executable stubs are linked into
text, and the dispatch slots and closure objects stay in writable data.
Callers must publish a prepared entry before using it across threads and
finish all calls before rebinding or freeing it. Allocation returns null
when all slots are occupied.

```sh
python3 tools/browser_glib_iconv.py --iconv-archive /path/to/libiconv-1.19.tar.gz \
  --glib-archive /path/to/glib-2.90.1.tar.xz \
  --pcre2-archive /path/to/pcre2-10.49.tar.bz2 \
  --libffi-archive /path/to/libffi-3.8.0.tar.gz --meson /path/to/meson
python3 tools/glib_platform_guest.py --iconv-prefix /path/to/run/target-iconv \
  --pcre2-prefix /path/to/run/target-pcre2 --libffi-prefix /path/to/run/target-libffi
```

## Target zlib

The browser build pins **zlib 1.4** with SHA-256
`02dd159783677a749c04adbdd02b55b8a2bcc7e054f51b76b549199a916ad584`.
This release includes upstream compression and gzip security fixes. The
shared SrvrPorts checkout still supplies zlib 1.3.2 to its existing consumers.
The browser recipe uses upstream `configure --static` with the SrvrOS C
wrapper and builds the compression and gzip objects into `libz.a`. It stages
both public headers, the generated target `zlib.pc` and the upstream license.
[zlib 1.4 release](https://github.com/madler/zlib/releases/tag/v1.4).

```sh
python3 tools/browser_glib_iconv.py --iconv-archive /path/to/libiconv-1.19.tar.gz \
  --glib-archive /path/to/glib-2.90.1.tar.xz \
  --pcre2-archive /path/to/pcre2-10.49.tar.bz2 \
  --libffi-archive /path/to/libffi-3.8.0.tar.gz \
  --zlib-archive /path/to/zlib-1.4.tar.gz --meson /path/to/meson
python3 tools/glib_platform_guest.py --iconv-prefix /path/to/run/target-iconv \
  --pcre2-prefix /path/to/run/target-pcre2 --libffi-prefix /path/to/run/target-libffi \
  --zlib-prefix /path/to/run/target-zlib
```

## Target proxy-libintl

GLib's own wrap pins **proxy-libintl 0.5** with SHA-256
`f7a1cbd7579baaf575c66f9d99fb6295e9b0684a28b095967cfda17857595303`.
The recipe places the checked source in GLib's subprojects directory and lets
Meson select its internal `intl` dependency with wrap downloads disabled.
An independent Meson build of the same directory supplies the static archive,
header and upstream license for the guest probe. The upstream `STUB_ONLY`
configuration implements message passthrough, singular/plural selection and
basic domain behavior. The tag is 0.5 and the Meson project reports version 1.
Upstream `setup()` uses an unsynchronized initialization flag, and
`textdomain()` stores mutable process-wide state. The serial guest probe
establishes the fallback behavior. Concurrent first use and domain mutation
need separate GLib integration checks.
[Pinned proxy-libintl source archive](https://github.com/frida/proxy-libintl/archive/refs/tags/0.5.tar.gz).

Add `--proxy-intl-archive /path/to/proxy-libintl-0.5.tar.gz` to the full
zlib configure command above. Add `--proxy-intl-prefix
/path/to/run/target-proxy-intl` to the corresponding guest command.

## GIO resolver header

SrvrOS exports `C_IN = 1` from
[`<arpa/nameser.h>`](../userspace/lib/include/arpa/nameser.h) through the
normal `srvros-sysroot` target. The header supplies the DNS Internet class.
[IANA DNS class registry](https://www.iana.org/assignments/dns-parameters#dns-parameters-2).

The historical resolver review chat was archived. The integration owner
confirmed the compile-time boundary, and the user delegated validation of
the header to this task. A fresh SDK export passed the exact GIO C probe
and strict standalone C11 and C++23 checks with repeated inclusion. The
libc archive, startup object and linker script retained their hashes.
Verification used target compiler probes, and the VM window stayed available.

The fresh Meson run reused the verified providers from the combined
proxy-libintl run with the rebuilt SDK. GIO's primary `C_IN` probe compiled.
That header-only run failed the direct `res_query()` check on missing
`<resolv.h>`. The static linker reported that the `resolv`, `bind` and
`socket` fallback libraries were unavailable. At runtime the GIO record resolver
consumes DNS wire answers and uses resolver error state and compressed-name
decoding. Its source includes SRV, TXT, SOA, NS and MX handling.
[Pinned GIO resolver source](https://github.com/GNOME/glib/blob/2.90.1/gio/gthreadedresolver.c).

## GIO raw resolver query

The networking owner's historical chat was archived and the integration
owner found no active successor. The user delegated review of the query
contract to this task. SrvrOS exports `res_query()` from `<resolv.h>` and
thread-local `h_errno` from `<netdb.h>`. The query uses the existing
bounded IPv4 UDP resolver transport with an independent socket and random
transaction ID. The existing A lookup retains its answer parser and native
fallback. The [POSIX profile](posix-supported-profile.md) describes the wire
and error contracts.

Meson 1.10.2 on the SDK rebuilt with main `b95bc096` links both
`res_query()` and `socket()` directly and exits 0 with Ninja files.
The targeted command
`ninja -C /path/to/glib-build gio/libgio-2.0.a.p/gthreadedresolver.c.o`
failed first at `dn_expand()` on that SDK and also reported missing
`GETSHORT`, `GETLONG`, `HEADER` and record constants.

`python3 tools/glib_platform_guest.py --resolver` passed twice in the
diskless Q35 guest using the restored SDK, kernel, init and Limine inputs.
The private Ethernet peer saw 24 queries and sent 30 replies. The native
fixture checked raw TXT and SRV replies, compressed names, short-buffer sizing,
rejection of unrelated or malformed replies, DNS error mapping, thread-local
`h_errno` and the existing IPv4 `getaddrinfo()` path. Both platform probes also
returned exit 0, and QEMU and its DNS peer stopped. Readback from the boot ISO
matched the restored kernel and the packed init and probe ELFs.
The [resolver receipt](assets/evidence/gio-res-query/receipt.json) records
the configure and guest evidence.

## GIO DNS wire decoding

PR #511 landed as `00ea6d78`. The follow-up adds bounded `dn_expand()`
and the nameser declarations used by pinned GIO. The native fixture tests
literal and compressed names, escaping, original bytes consumed, capacity
limits and malformed input. It also expands a compressed owner from a
live SRV fixture reply. Two resolver runs and two platform runs exited 0 in
the diskless guest. QEMU and the DNS peer stopped.

Fresh Meson configuration and the exact GIO resolver object build both
passed. On that SDK, the static GLib/GIO build first stopped compiling
`glib/gfileutils.c:1505`, where the `O_NOFOLLOW` failure path references
`ELOOP`. See the [wire decoder receipt](assets/evidence/gio-dns-wire/receipt.json).

## GLib file open policy

PR #515 landed as `77b0c7cb`. The filesystem owner reviewed the invariant:
initramfs skips archive links and exFAT exposes only files and directories.
The SDK exports `ELOOP` and its error text for GLib's no-follow open path.

Both `--file-policy` guest runs confirmed that the archive link was absent
with ordinary and no-follow opens and the target bytes were intact. The
private exFAT disk mounted at `/fat` and supported directory-relative no-follow
create, exclusive-create refusal, truncate, replacement and cleanup. Both
platform probes also exited 0. QEMU stopped and the VM window was released.

Fresh configure passed and `gfileutils.c` compiled. The static build on that
SDK stopped at `glib/ghostutils.c:426` using `_POSIX_HOST_NAME_MAX`. See the
[file-policy receipt](assets/evidence/glib-file-policy/receipt.json).

## GLib hostname limits

PR #517 landed as `0273ba64`. The pinned GLib C build explicitly includes the
SDK's `<limits.h>` so the hostname utility can use `HOST_NAME_MAX`. The
ordinary include resolves to Zig's freestanding header, which lacks that
macro. The scoped build keeps the compiler's existing header search order.

Two native platform runs verified `sysconf(_SC_HOST_NAME_MAX) == 63`,
`gethostname() == "srvros"` matching `uname().nodename`, a terminated one-byte
result and `EINVAL` for a zero-length buffer. Both runs exited 0 and QEMU
stopped. Fresh Meson configure and the `ghostutils.c` object build passed.
That static build first stopped at `NSIG` and `SA_NOCLDSTOP` in
`glib/gmain.c`.
See the [hostname receipt](assets/evidence/glib-hostname/receipt.json).

## GLib Unix child watches

The pinned GLib patch uses the SDK signal-mask width for its tables and
selects `SA_RESTART` for SrvrOS handler registration. A failed registration
returns a failed watch creation without adding a watch or reference.
GLib's wakeup uses its existing nonblocking pipe path because SrvrOS's
synthetic eventfd is mutex-backed. Signal delivery uses the SDK's
existing libc checkpoints.

Two diskless native probes verified that an unmasked caller at a libc poll
checkpoint handled `SIGCHLD` and woke a worker that inherited a blocked mask.
The worker reaped the watched PID with exit 17. The earlier unwatched child
remained independently reapable with exit 42. Both platform probes also
passed, QEMU stopped, and the window was released.

Fresh configure and the `gmain.c` and `gwakeup.c` objects passed. That static
build first stopped at missing `creat()` in `glib/gstdio.c:1147`. The focused
`glib-unix.c` build also found a `GPid`/`pid_t` ABI mismatch and an absent
descriptor-path backend. These gates block a guest test linked against
target GLib's `GChildWatchSource`. See the
[Unix watch receipt](assets/evidence/glib-unix-watch/receipt.json).

## GLib file creation

The filesystem owner reviewed the SDK `creat()` wrapper. It calls the
existing `open()` implementation with `O_WRONLY | O_CREAT | O_TRUNC` and
passes the supported permission bits as an `int`.

Two native runs on a disposable exFAT disk verified mode 0640 for a new
file requested as 0666 under umask 0027, a write-only descriptor with
`read()` returning `EBADF`, and no close-on-exec flag. A second `creat()`
truncated the file to zero bytes and kept mode 0640 despite a different
requested mode. The probe read back rewritten data, checked `EISDIR` for a
directory, and compared missing-parent behavior with equivalent `open()`.
Both platform probes and both file-policy probes exited zero. Boot ISO
readback matched the kernel and all packed executables, and the guest
removed its test directory. QEMU stopped and the VM window was released.

Fresh configure, the `gstdio.c` object, and the host FD I/O restart test
passed. That static GLib/GIO build stopped in `glib-unix.c` because GLib's
`GPid` (`int`) and the SDK's `pid_t` (`long`) differ in size and
alignment, and `g_unix_fd_query_path()` had no target branch.
See the [file creation receipt](assets/evidence/glib-creat/receipt.json).

PR #522 was requalified on main `6f4c8b92` at source `dac47528` with a
rebuilt SDK, kernel and init. The new private exFAT guest passed both
file-policy runs and both platform runs. ISO readback matched all boot
payloads, and a read-only check confirmed the test entries were removed
from the disposable disk. The host FD I/O restart regression passed.
Reusing Meson's recorded commands with the rebuilt SDK, `gstdio.c`
compiled and `glib-unix.c` reproduced the three diagnostics above.
See the [current-main receipt](assets/evidence/glib-creat/current-main-receipt.json).

## Linked GLib child watch

The port uses GLib's detected 64-bit C type and format for SrvrOS `GPid`.
The generated header matches the SDK's `pid_t` (`signed long`) and reports
`G_PID_FORMAT` as `"li"`. GLib's trapped-test diagnostics use that macro.
The wait-status decoder handles the SDK's exit and signal results and
compiles the stopped-child branch where its wait macros exist.

The filesystem owner reviewed the explicit `G_FILE_ERROR_NOSYS` result
for `g_unix_fd_query_path()`. The SDK can resolve directory handles for
its `*at()` APIs. The checksum-verified WPEWebKit 2.54.2 source had no caller
of the general descriptor-path API, and GLib's own references were in
tests. The guest checked a valid `/init` descriptor with and without an
error output. [Pinned WPE release checksum](https://wpewebkit.org/release/wpewebkit-2.54.2.html)

The static executable checks exact `GPid *` compatibility with `pid_t *`
and formats a PID value above 32 bits. It observes an unrelated child exit
without reaping it, attaches a real `GChildWatchSource`, and releases the
watched child after the initial main-context iteration. The callback
fires exactly once with the watched PID and exit status 17. The watched
child is already reaped, and native `waitpid()` separately returns the
unrelated child with status 42. The linked probe and both native
regression probes each ran twice, with six zero exits. ISO readback matched
the kernel and four packed executables. QEMU stopped and the VM window
was released.

The run used the matched SDK and boot inputs verified for PR #522.
Fresh Meson configuration and static GLib, GObject and GModule builds
passed. GLib's test diagnostics still warn on the SDK's separate
`PRI*64` format macros. That static GIO build stopped at the missing
`<fnmatch.h>` in `gio/xdgmime/xdgmimealias.c:22`.
See the [linked child-watch receipt](assets/evidence/glib-unix-abi/receipt.json).

With an offline configured tree from `tools/browser_glib_iconv.py`,
use its generated GLib headers and the target archives from the same run.
Set `stage` to that configure run's output directory.

```sh
src="$stage/glib-2.90.1"
obj="$stage/glib-build"
ninja -C "$obj" glib/libglib-2.0.a subprojects/proxy-libintl-0.5/libintl.a
ports/srvros/webkit/cc -std=gnu11 -O2 -Wall -Wextra -Werror -Wformat=2 \
  -DGLIB_STATIC_COMPILATION -I"$src" -I"$obj" -I"$src/glib" -I"$obj/glib" \
  -c tools/test_glib_linked_child_watch_guest.c -o "$stage/linked-child-watch.o"
ports/srvros/webkit/cc "$stage/linked-child-watch.o" "$obj/glib/libglib-2.0.a" \
  "$obj/subprojects/proxy-libintl-0.5/libintl.a" "$stage/target-pcre2/lib/libpcre2-8.a" \
  "$stage/target-iconv/lib/libiconv.a" -latomic -pthread -o "$stage/linked-child-watch.elf"
python3 tools/glib_platform_guest.py --child-watch \
  --linked-child-watch "$stage/linked-child-watch.elf"
```

## XDG MIME filename patterns

The public SDK `<fnmatch.h>` exports `fnmatch()` for the single-byte C
locale. It handles literal bytes, `*`, `?`, escaping, sets and ranges,
negated sets, ASCII character classes, and single-byte collating and
equivalence entries. The public flags control slash, leading period,
backslash and ASCII case folding. The matcher uses constant auxiliary
space.

The host ASan/UBSan suite passed 58 cases and an alternating-star match
and near miss. A C-locale comparison against the host matcher agreed on
1,386,528 generated pattern, filename and flag combinations. The SDK
export audit found only the new `fnmatch` symbol compared with the exact
prior archive. Its 37 unrelated missing entries were unchanged.

The diskless guest ran both the `fnmatch` and platform probes twice with
four zero exits. It checked complex MIME-style patterns with flags zero,
then exercised the public flags and byte-oriented locale behavior. The
kernel and three packed executables matched the boot ISO readback.
QEMU stopped and the VM window was released. Reproduce with the built
SDK, init and boot inputs using `python3 tools/glib_platform_guest.py --fnmatch`.

Fresh configure and all five XDG MIME objects that include
`<fnmatch.h>` passed. The two actual GIO callers are in
`xdgmimecache.c` and `xdgmimeglob.c`. That static GIO build failed
in `gio/inotify/inotify-kernel.c` at the undeclared `FIONREAD` and
`inotify_init()`. Meson auto-selects inotify because its
`inotify_init1()` link probe succeeds. The SDK function returns
`ENOSYS` at runtime. See the [fnmatch receipt](assets/evidence/glib-fnmatch/receipt.json).

## Local file monitoring

The SrvrOS port selects GIO's existing five-second individual-file polling
fallback in the automatic backend choice. The SDK's `inotify_init1()`
continues to link and return `ENOSYS`. A fresh configure recorded the polling
selection and generated no native monitor backend defines or inotify targets.

Two linked SrvrOS probes on a disposable exFAT disk verified the
`GLocalFile` and `GPollFileMonitor` types. Public GIO directory monitoring
returned `G_IO_ERROR_FAILED`. The file monitor emitted creation and a done
hint, a change and a done hint after an in-place equal-size rewrite with
a confirmed newer mtime, and deletion. It stayed quiet for seven seconds
after cancellation and recreation. The fixture was removed before exit.
Both platform controls also passed. Boot ISO readback matched the kernel
and all three packed executables. QEMU stopped and the VM window was released.

The guest uses a private local-file link because full static GIO is still
blocked. It retains actual GFile dispatch, local VFS registration, local
monitor extension lookup, metadata queries, GTask and the upstream polling
implementation. The proof-only build supplies declarations for existing
SDK fields, limits module registration to local services, and makes every
unavailable network, D-Bus or mount reference fault if called. These inputs
are recorded separately from the production configure and build. The
filesystem owner reviewed the scope and independently verified the stopped
guest and ISO readback.

Size and mtime changes are observable once sampled. SrvrOS reports
whole-second mtime, so same-size writes in one second and short-lived
changes between polls can be missed. A done hint describes observation,
and provides no storage durability guarantee.

That full build reached `gio/ginetaddress.c:461` and stopped at
`AI_NUMERICHOST`, followed by missing IPv6 address constants and
predicates. See the [polling build and guest receipt](assets/evidence/gio-poll-monitor/receipt.json).

## GIO address declarations

PR #527 retained the bundled GNU matcher in the production and diagnostic
Grep and Diffutils links. Integration verified all four links in full
builds at the merge and at main `54b1596c`. The address probe uses the
SDK, kernel and init rebuilt on that newer baseline.
[Earlier CLI receipt](assets/evidence/earlier-cli/receipt.json)

The SDK supplies the IPv4 multicast predicate, IPv6 unspecified and
loopback data, and IPv6 address classification over the network-order
bytes. The classifiers cover link-local and site-local prefixes and the
multicast scopes used by GIO. `getaddrinfo()` supports numeric IPv4 lookup with
`AI_NUMERICHOST` and returns `EAI_NONAME` for nonnumeric input. A numeric
request for an unsupported family returns `EAI_FAMILY` before either
resolver path can run. [RFC 3493](https://www.rfc-editor.org/rfc/rfc3493) ·
[RFC 4291](https://www.rfc-editor.org/rfc/rfc4291)

Integration found the historical networking chat archived and identified
no active successor. The filesystem owner independently reviewed the
SDK contract and the stopped guest. The host socket suite passed 55
modes. Numeric cases left both the UDP DNS and native DNS fallback
counters at zero. The C and C++ target header checks passed.

Both address and both platform probes passed in the normal diskless
guest. The native probes checked the IPv4 multicast boundaries, exact
IPv6 constants, prefix and scope classification with single evaluation,
numeric IPv4 result shape and both numeric error cases. ISO readback
matched the kernel and all three packed executables. QEMU stopped and
the VM window was released.

The fresh post-guest configure retained the polling fallback. Static GIO
compiled `ginetaddress.c`, `ginetaddressmask.c` and `ginetsocketaddress.c`.
The next failed unit was `gio/giptosmessage.c`, at `IP_TOS` on lines 73
and 87. [Address build and guest receipt](assets/evidence/gio-inet-address/receipt.json)

## IPv4 traffic class messages

The topic starts from merged OS #528 at `8c5565d4` and reuses the verified
kernel, init, SDK and static providers. On SrvrOS, `giptosmessage.c` includes
`<netinet/ip.h>` for the SDK's existing `IP_TOS` constant. The full build
compiles the source with the pinned target configuration.

Two guest runs verified all 256 DSCP and ECN combinations, the one-byte
level/type/size contract, and refusal of malformed control messages. The
focused link uses the real IPv4 message and base class objects, with the
probe calling the IPv4 class decoder directly.
The generic ancillary-message dispatcher, packet marking and ancillary
reception are outside the test. The two platform control runs also passed.

The first boot lacked a NIC, so creating a UDP socket failed with SDK
`EMFILE`. The successful retry reused the exact codec ELF in a diskless guest
with a private e1000 on an idle local socketpair. On a valid IPv4 UDP socket,
both `getsockopt()` and `setsockopt()` for `IP_TOS` returned `ENOSYS`.
ISO readback matched the kernel and all three packed executables. QEMU
stopped and the VM window was released.

After guest shutdown, the pinned static GIO build stopped at the undeclared
`IPV6_TCLASS` on lines 73 and 87 of `gipv6tclassmessage.c`.
See the [codec and build receipt](assets/evidence/gio-ip-tos/receipt.json)
and the [upstream GIPTosMessage API](https://docs.gtk.org/gio/class.IPTosMessage.html).

## IPv6 traffic class messages

From merged OS #530 at `66fbf299`, GIO's `gipv6tclassmessage.c` includes
the SDK's `<netinet/ip6.h>` on SrvrOS for its existing `IPV6_TCLASS`
declaration. The standard target graph compiles the source. The focused
guest link uses the real IPv6 codec and GSocketControlMessage base
objects and calls the IPv6 class decoder directly.

Two runs verified all 256 DSCP/ECN combinations and the aligned native
`int` payload. The decoder rejected incorrect levels, types and sizes.
IPv6 UDP and TCP socket creation returned `EAFNOSUPPORT`. This proof
covers the class codec and the SDK's socket refusal. The generic
ancillary-message dispatcher and IPv6 transport are outside its scope.

Both platform controls also passed. The guest used one CPU and 512 MiB
with no disk or NIC. ISO readback matched the kernel and all three
packed executables. The guest stopped and the VM window was released.

The post-guest static GIO build stopped at the undeclared `inet_aton()` call
in `gio/gresolver.c:461`.
See the [codec and build receipt](assets/evidence/gio-ipv6-tclass/receipt.json)
and the [upstream GIPv6TclassMessage API](https://docs.gtk.org/gio/class.IPv6TclassMessage.html).

## GIO IPv4 numbers-and-dots parsing

From merged OS #531 at `0117e6cf`, the SDK exports `inet_aton()` in
`<arpa/inet.h>`. It accepts one to four IPv4 parts in decimal, octal
or hexadecimal and returns network-order bytes. It checks component
widths and overflow, allows trailing ASCII whitespace, and rejects
trailing text. A null destination validates the input. Failed parses
preserve the destination and `errno`. [OpenBSD interface reference](https://man.openbsd.org/inet_aton.3)

GResolver checks IPv4 spelling before returning an address because the
SDK's `inet_pton()` accepts padded decimal octets. GResolver rejects those
spellings and uses `inet_aton()` to identify abbreviated and mixed-base
forms. The SDK's `inet_pton()` and existing resolver entry points retain
their previous parsing behavior.

Two guest runs passed the SDK cases. Two linked public GResolver runs
returned canonical IPv4, rejected five legacy or padded forms with
`NOT_FOUND` before calling the backend, and sent an ordinary hostname
to the test backend exactly once. Both platform controls passed. ISO
readback matched the kernel and executables. The diskless guest stopped
and the VM window was released.

The host parser passed address and undefined-behavior sanitizer checks.
All 55 socket and 23 libc DNS cases passed. A target C++ object linked
against the new C symbol. The ARM host could not compile the x86 kernel
DNS fixture.

The production static target compiled `gresolver.c` and next stopped in
`gsocket.c`. The first error was the undeclared `SOCK_SEQPACKET` at line 523.
The compiler also reported missing errno, backlog and socket-option
declarations. [Verification receipt](assets/evidence/gio-inet-aton/receipt.json)

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
- **Configure with PCRE2.** Meson resolved all six target poll constants,
  detected iconv and PCRE2 10.49, and configured the bundled GVDB sources.
  It stopped at the required libffi lookup before building GLib.
- **Configure with libffi.** Meson detected libffi 3.8.0 from the staged
  target package and stopped at zlib.
- **Configure with zlib.** Meson detected zlib 1.4 from the isolated target
  package and continued to `intl`. Both builtin and system checks failed,
  then Meson stopped at the proxy-libintl fallback because wrap downloads
  were disabled.
- **Configure with proxy-libintl.** Meson selected its internal `intl`
  dependency and continued into GIO. Both public-header probes for `C_IN`
  failed with a missing `<arpa/nameser.h>`. Meson stopped before producing
  the complete GLib build graph.
- **Configure with the exported resolver header.** The rebuilt SDK passes
  the primary `C_IN` check. The first remaining configure stop is
  `Could not find res_query()` in GIO.
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
- **Target regular expressions.** The static PCRE2 fixture verified the
  reported version, 8-bit-only width, Unicode support and disabled JIT.
  It exercised named and numeric Unicode capture offsets, no match, invalid
  UTF-8, a malformed pattern and the match-limit result. Two executions
  each of PCRE2, iconv and the native wait fixture returned exit 0 and the
  monitor prompt. The guest stopped. A relink using the isolated pkg-config
  flags produced the exact PCRE2 executable bytes extracted from the boot ISO.
- **Target foreign calls and callbacks.** The libffi probe covered register
  spill, variadic promotions, a mixed aggregate return, System V and Win64
  callbacks, legacy raw and Java-raw callbacks, thread publication, slot
  exhaustion and reuse. All four fixtures passed twice in the same normal
  guest, which stopped after eight zero exits. Relinking with the target
  pkg-config flags reproduced the boot ISO's libffi probe bytes. The ELF
  places all 128 stubs in a read-execute segment and all dispatch slots in
  non-executable writable data.
- **Target compression.** The zlib fixture verified the header and runtime
  versions, known CRC32 and Adler32 vectors, one-shot compression, chunked
  gzip streaming, corrupt and truncated stream errors, and blocking gzip
  file APIs over a pipe. All five fixtures passed twice. The guest stopped
  after ten zero exits. A relink with the target pkg-config flags reproduced
  the exact zlib probe bytes extracted from the boot ISO.
- **Target translation fallback.** The proxy-libintl fixture exercised
  single-threaded identity and context passthrough, singular/plural selection,
  default and replaced text domains, and the upstream binding and codeset
  behavior. All six fixtures passed twice. The diskless guest stopped after
  twelve zero exits. Relinking with the verified static archive reproduced
  the exact proxy-libintl probe bytes extracted from the boot ISO.
- **Host GLib.** Homebrew's GLib 2.90.1 passed an independent real
  `GMainContext` test using acquire, prepare, query, poll, check, and dispatch.
  One owner processed a worker-attached source and a timer. The quiet wait
  slept for about 83 ms and the wake/timer portion completed in two sleeping
  iterations. This verifies the algorithm on the host. Running actual GLib
  within the SrvrOS guest awaits target library bring-up.
  [GLib main-loop API](https://docs.gtk.org/glib/main-loop.html).

The [original receipt](assets/evidence/glib-prereq/receipt.json),
[poll and iconv receipt](assets/evidence/glib-iconv/receipt.json),
[PCRE2 receipt](assets/evidence/glib-pcre2/receipt.json),
[libffi receipt](assets/evidence/glib-libffi/receipt.json),
[zlib receipt](assets/evidence/glib-zlib/receipt.json),
[proxy-libintl receipt](assets/evidence/glib-proxy-intl/receipt.json), and
[GIO header receipt](assets/evidence/gio-nameser/receipt.json) record
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
