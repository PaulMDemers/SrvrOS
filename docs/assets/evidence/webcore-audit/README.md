# WebCore audit evidence

This is a source/configuration audit at OS88bdc382, not an engine build or
rendering result. `receipt.json` identifies the existing pinned2.54.2 archive,
31 inspected upstream files and15 local contract files by SHA256. The upstream
hashes were checked directly against archive members after inspection. No
upstream source is vendored in this collection.

`candidate-profile.json` is a proposed configuration and runtime policy. Its42
configure names occur in the pinned CMake source; a full target configure has
not validated their combination, toolchain or dependency closure. Minimum
versions in the audit are not selected pins for new ports.

`package-probe/CMakeLists.txt` invokes the actual pinned FindGLib module with
no enabled compiler and an empty target prefix. The retained invocation exits1
with `glib-2.0 is required`. It does not compile or link anything. The collector
initially expected a generic CMake diagnostic; the receipt records that mismatch
and the actual custom module error without a retry or successful-package stub.
`package-probe.log` replaces only the local checkout prefix with `$SRVROS_ROOT`;
the receipt binds both original and published bytes.

The source-backed CPU path and shared-memory/runtime blockers are explained in
[the audit](../../../browser-webcore-audit.md). No full WPE target configure,
dependency/engine compile, target execution or actual rendered page is claimed.
