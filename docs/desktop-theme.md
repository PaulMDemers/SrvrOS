# Global desktop appearance

Settings → Appearance (below Sound) changes Blue, Graphite or Teal accent and
Solid, Subtle or Clear window glass. Selection and focus colors follow the accent;
window title bars, menu bar and dock follow the glass treatment. App content stays
opaque. This is tinted transparency, highlights and bounded fading shadows, not
backdrop blur. Solid remains available for predictable contrast.

## Ownership and updates

The compositor owns the authoritative `desk_preferences` snapshot and asynchronous
persistence. A focused Settings window sends masked changes; every accepted change
is broadcast to registered windows, and new windows receive the current snapshot.
Rejected and unchanged requests receive authoritative state too. Settings reports
Applying, Saved, session-only or conflict status rather than claiming a successful
save when transport or storage failed. Its appearance confirmation has a bounded
timeout; retry remains explicit.

Protocol v16 appends `theme_accent` and `glass_level` to the preference payload
inside the unchanged 168-byte desktop packet. Build matched compositor and GUI
applications; no mixed-version compatibility is claimed. The strict v5 preference
file fits the existing 256-byte bound and reads v1–v4 with Blue/Subtle defaults.
The existing delayed-load merge, debounce, verified write, conflict and shutdown
rules remain in the OS runtime.

Each `desk_app` caches a validated, server-authenticated preference snapshot.
Visual changes dirty that window. The inline `desk_app_theme()` adapter resolves
a caller-owned theme through SrvrUI's `desk_theme_configure()`; no mutable theme
global or OS calls enter the widget library. App main windows and owned dialogs
resolve their own snapshots. Theme changes do not rewrite image, paint or terminal
content colors. Compact control fonts retain their established size; the existing
document text-size behavior remains app-owned. Color-only updates avoid relayout
and preserve selection, scroll position and pointer capture.

The first engine exposes bounded presets. Future properties should extend this
same validated snapshot/resolver path, with a protocol/file migration when the
wire or saved representation changes. Avoid new per-app preference stores.

## Composition and verification

The glass primitive blends only the current destination pixel, with coordinates
anchored to the original rectangle and bounded rounded spans. It preserves incoming
clips and padded strides. No neighboring pixel read makes partial damage depend
on tile order. Translucent chrome and shadows never count as opaque occluders;
only a higher window's opaque body can suppress covered client pixel copies and
opaque body fills. Fully hidden windows may be skipped only when their entire
paint bounds fall inside that higher opaque body.

`test-widget-theme` and the scene, overdraw and session fixtures under
`test-desktop` cover blending, clipping, all glass levels, snapshot rejection and redraw.
The complete desktop/widget/I/O suite is the integration gate. The application VM
checks a selected Text row before and after a live accent change, a new window,
verified saved config and a new desktop process loading that config. The Settings
VM retains existing clock, wallpaper, pointer and persistence coverage.

Use `make widget-theme-preview` for real control-state specimens. Runtime screenshots
show appearance, but cannot prove physical scanout is tear-free; see the
[presentation investigation and driver plan](display-presentation.md).


Glass has a CPU cost in the software compositor. The macOS/Rosetta 128-window
stacked full-scene fixture measured 5,516,676 writes and a roughly 8.8 ms median
with occlusion, versus 23,684,280 writes and roughly 16.4 ms without it. The prior
opaque renderer was roughly 0.12 ms in that fixture because it could discard
whole covered windows. These are stress-fixture host numbers, not native input
latency or a guarantee of frame rate. Caching or accelerated composition is a
separate performance direction; correctness requires retaining visible glass
layers. Ordinary application workflows are checked in QEMU.


## Verified result — November 2, 2026

Runtime `db898927`; SrvrUI `df2e311`, SrvrApps `e4b9235`, SrvrPorts `0494ae2`.
Full matched build, desktop/widget/I/O host suites, theme ASan/UBSan and final
application/compositor VMs pass. The separate Settings VM passed before the last
title-frost-only adjustment. The host scene test includes preserved-buffer drag
and lower-window content changes under glass, not only static tiled rendering.
The live application check proves existing and new window selection colors and
saved appearance reload. All disposable VMs stopped; native scanout is unverified.

![Teal Clear desktop with live Settings](assets/screenshots/glass-desktop.png)

![Open Text window after the global appearance update](assets/screenshots/glass-text.png)

![Blue, Graphite and Teal controls rendered by SrvrUI](assets/screenshots/glass-widgets.png)

[Image hashes and source revisions](assets/screenshots/glass-manifest.json).
