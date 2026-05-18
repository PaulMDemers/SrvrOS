# srvros Tool Inventory

This inventory reflects the command names installed by the current Makefile into
the generated exFAT image and initramfs command set. It is the quick reference
for checking whether README and architecture notes match what the build ships.

The generated image also installs `/init` separately for system startup; the
commands below are the user-facing shell/PATH surface.

## Shell Builtins

`sh` currently provides these builtins:

```text
help man apropos exit exec return shift set source . path cd pwd clear echo env
export unset alias history type which command test [ break continue jobs wait fg
bg kill service dhcp net dns rmdir read :
```

## Core Commands

Basic shell and system inspection commands:

```text
sh hello cat more echo write clear ps kill which env pwd true false sleep date
touch mktemp basename dirname uname hostname uptime id whoami
```

The current `date` command reports monotonic uptime-style time because srvros
does not yet have RTC or network wall-clock plumbing.

## File And Filesystem Commands

File, directory, metadata, and disk-use commands:

```text
ls stat chmod cp rm mkdir mv find du df realpath readlink sync ln tty stty
```

`ln` is installed as part of the shared POSIX utility applet, but currently
reports that links are unsupported because srvros has not grown hard-link or
symlink metadata yet.

## Text And Pipeline Commands

Text processing, comparison, stream, and data inspection commands:

```text
wc grep head tail tee tap sort uniq cut xargs seq cmp yes diff patch sed expr
printf tr cksum sum comm paste join split od hexdump strings file dd
```

`tap` reads from stdin, writes the stream to stdout, and also mirrors it to a
secondary file.

## Archive, Build, And Porting Commands

Archive and early source-port/build-script helpers:

```text
install tar gzip gunzip minizip miniunz make byacc
```

Bundled third-party port demos and interpreters:

```text
zlibdemo jsondemo inidemo linedemo sqlitedemo uvdemo lua
```

`gzip` and `gunzip` are the same zlib-backed program installed under two names.
`minizip` and `miniunz` are built from zlib contrib MiniZip sources.
`uvdemo` uses the srvros `uv.h` compatibility shim as the first libuv-shaped
bring-up target, covering timers, filesystem requests, async/work callbacks, fd
polling, UDP, and TCP server flow.

## Network Commands

Guest-side networking diagnostics, clients, and socket probes:

```text
webd httpget udpdns udpecho netstat ifconfig route arp ping host netcheck netabi
```

The shell also has `dhcp`, `net`, and `dns` builtins for the kernel DHCP,
network-status, and DNS resolver paths.

## Services And Process Helpers

Background service and process-control helpers:

```text
svscan time timeout nohup nice
```

`service` is a shell builtin that controls `/fat/etc/services/*.svc`; `svscan`
is the boot/startup supervisor process used by `/fat/etc/init.sh`.

## GUI And Interactive Demos

Framebuffer/UI experiments and GUI clients:

```text
ui desktop calcgui notesgui textedit imgedit
```

## ABI, POSIX, And Stress Probes

Regression and compatibility probes used by smoke tests:

```text
sysabi spin fpdemo posixdemo threadstress execdemo fdprobe lockprobe ttydemo
```

These are intentionally shipped in the generated image so QEMU smoke tests can
exercise the same binaries a user can launch from `/fat/bin`.

## Shared POSIX Utility Applet Names

One `posixutils` binary is installed under multiple command names:

```text
ln sync test [ cksum sum comm paste join split od hexdump strings file tty stty
time timeout nohup nice
```

The image builder deduplicates identical applet payloads by source ELF so these
aliases share storage instead of consuming a full copy per command name.
