.SUFFIXES:

ARCH := x86_64
KERNEL := build/kernel/srvros.elf
IMAGE := build/srvros-$(ARCH).iso
USB_IMAGE := build/srvros-usb.img
ISO_ROOT := build/iso_root
INITRAMFS_ROOT := build/initramfs_root
INITRAMFS := build/initramfs.tar
USER_INIT := build/userspace/init.elf
USER_HELLO := build/userspace/hello.elf
USER_CAT := build/userspace/cat.elf
USER_MORE := build/userspace/more.elf
USER_SH := build/userspace/sh.elf
USER_LS := build/userspace/ls.elf
USER_ECHO := build/userspace/echo.elf
USER_WRITE := build/userspace/write.elf
USER_WC := build/userspace/wc.elf
USER_CLEAR := build/userspace/clear.elf
USER_PS := build/userspace/ps.elf
USER_KILL := build/userspace/kill.elf
USER_WHICH := build/userspace/which.elf
USER_ENV := build/userspace/env.elf
USER_PWD := build/userspace/pwd.elf
USER_TRUE := build/userspace/true.elf
USER_FALSE := build/userspace/false.elf
USER_SLEEP := build/userspace/sleep.elf
USER_DATE := build/userspace/date.elf
USER_TOUCH := build/userspace/touch.elf
USER_MKTEMP := build/userspace/mktemp.elf
USER_BASENAME := build/userspace/basename.elf
USER_DIRNAME := build/userspace/dirname.elf
USER_GREP := build/userspace/grep.elf
USER_HEAD := build/userspace/head.elf
USER_TAIL := build/userspace/tail.elf
USER_TEE := build/userspace/tee.elf
USER_UNAME := build/userspace/uname.elf
USER_HOSTNAME := build/userspace/hostname.elf
USER_UPTIME := build/userspace/uptime.elf
USER_FIND := build/userspace/find.elf
USER_DU := build/userspace/du.elf
USER_DF := build/userspace/df.elf
USER_SORT := build/userspace/sort.elf
USER_UNIQ := build/userspace/uniq.elf
USER_CUT := build/userspace/cut.elf
USER_XARGS := build/userspace/xargs.elf
USER_SEQ := build/userspace/seq.elf
USER_REALPATH := build/userspace/realpath.elf
USER_ID := build/userspace/id.elf
USER_WHOAMI := build/userspace/whoami.elf
USER_READLINK := build/userspace/readlink.elf
USER_CMP := build/userspace/cmp.elf
USER_YES := build/userspace/yes.elf
USER_INSTALL := build/userspace/install.elf
USER_DIFF := build/userspace/diff.elf
USER_TAR := build/userspace/tar.elf
USER_GZIP := build/userspace/gzip.elf
USER_MINIZIP := build/userspace/minizip.elf
USER_MINIUNZ := build/userspace/miniunz.elf
USER_PATCH := build/userspace/patch.elf
USER_MAKE := build/userspace/make.elf
USER_BYACC := build/userspace/byacc.elf
USER_SED := build/userspace/sed.elf
USER_EXPR := build/userspace/expr.elf
USER_PRINTF := build/userspace/printf.elf
USER_TR := build/userspace/tr.elf
USER_DD := build/userspace/dd.elf
USER_ED := build/userspace/ed.elf
USER_POSIXUTILS := build/userspace/posixutils.elf
USER_STAT := build/userspace/stat.elf
USER_CHMOD := build/userspace/chmod.elf
USER_CP := build/userspace/cp.elf
USER_RM := build/userspace/rm.elf
USER_MKDIR := build/userspace/mkdir.elf
USER_MV := build/userspace/mv.elf
USER_TAP := build/userspace/tap.elf
USER_SVSCAN := build/userspace/svscan.elf
USER_WEBD := build/userspace/webd.elf
USER_PLAYWAV := build/userspace/playwav.elf
USER_HTTPGET := build/userspace/httpget.elf
USER_UDPDNS := build/userspace/udpdns.elf
USER_UDPECHO := build/userspace/udpecho.elf
USER_NETSTAT := build/userspace/netstat.elf
USER_IFCONFIG := build/userspace/ifconfig.elf
USER_ROUTE := build/userspace/route.elf
USER_ARP := build/userspace/arp.elf
USER_PING := build/userspace/ping.elf
USER_HOST := build/userspace/host.elf
USER_NETCHECK := build/userspace/netcheck.elf
USER_NETABI := build/userspace/netabi.elf
USER_TCPSTRESS := build/userspace/tcpstress.elf
USER_TCPPROBE := build/userspace/tcpprobe.elf
USER_SYSABI := build/userspace/sysabi.elf
USER_SPIN := build/userspace/spin.elf
USER_FPDEMO := build/userspace/fpdemo.elf
USER_UI := build/userspace/ui.elf
USER_DESKTOP := build/userspace/desktop.elf
USER_DISPLAYD := build/userspace/displayd.elf
USER_DESKSTUDY := build/userspace/deskstudy.elf
USER_DESKD := build/userspace/deskd.elf
USER_DESKCLIENT := build/userspace/deskclient.elf
USER_DESKIO := build/userspace/deskio.elf
USER_DESKTOOLS := build/userspace/desktools.elf
USER_DESKTERM := build/userspace/deskterm.elf
USER_CLOCKPROBE := build/userspace/clockprobe.elf
USER_LIFETIMEPROBE := build/userspace/lifetimeprobe.elf
USER_PTYPROBE := build/userspace/ptyprobe.elf
USER_DESKSKETCH := build/userspace/desksketch.elf
USER_SHOTWORKER := build/userspace/shotworker.elf
USER_DESKFS := build/userspace/deskfs.elf
USER_IMAGEWORKER := build/userspace/imageworker.elf
USER_DESKFIND := build/userspace/deskfind.elf
USER_FINDFSWORKER := build/userspace/deskfind-worker.elf
USER_DESKNET := build/userspace/desknet.elf
USER_DESKCLOCK := build/userspace/deskclock.elf
USER_DESKHELP := build/userspace/deskhelp.elf
USER_DESKVIEW := build/userspace/deskview.elf
USER_GUI := build/userspace/gui.elf
USER_SURFACEDEMO := build/userspace/surfacedemo.elf
USER_GUI2DEMO := build/userspace/gui2demo.elf
USER_GUIFAIL := build/userspace/guifail.elf
USER_CALC := build/userspace/calc.elf
USER_NOTES := build/userspace/notes.elf
USER_TEXTEDIT := build/userspace/textedit.elf
USER_PAINT := build/userspace/paint.elf
USER_FILEMAN := build/userspace/fileman.elf
USER_POSIXDEMO := build/userspace/posixdemo.elf
USER_THREADSTRESS := build/userspace/threadstress.elf
USER_EXECDEMO := build/userspace/execdemo.elf
USER_FDPROBE := build/userspace/fdprobe.elf
USER_LOCKPROBE := build/userspace/lockprobe.elf
USER_TTYDEMO := build/userspace/ttydemo.elf
USER_JSONDEMO := build/userspace/jsondemo.elf
USER_INIDEMO := build/userspace/inidemo.elf
USER_LINEDEMO := build/userspace/linedemo.elf
USER_SQLITEDEMO := build/userspace/sqlitedemo.elf
USER_ZLIBDEMO := build/userspace/zlibdemo.elf
USER_LUA := build/userspace/lua.elf
USER_UVDEMO := build/userspace/uvdemo.elf
USER_LIBUVDEMO := build/userspace/libuvdemo.elf
USER_NODEPROBE := build/userspace/nodeprobe.elf
USER_LIBCPROBE := build/userspace/libcprobe.elf
USER_TLSPROBE := build/userspace/tlsprobe.elf
USER_CXXPROBE := build/userspace/cxxprobe.elf
USER_CXXSTLPROBE := build/userspace/cxxstlprobe.elf
NODE_RUNTIME_ELF := build/node-srvros-link-probe/node-srvros.elf
NODE_RUNTIME_STRIPPED := build/node-srvros-runtime/node-srvros-stripped.elf
NODE_RUNTIME_EXFAT := build/node-srvros-runtime/srvros-node.exfat
EXFAT_IMAGE := build/srvros.exfat
SECOND_EXFAT_IMAGE := build/srvros-secondary.exfat

LIMINE_DIR := build/limine-binary
LIMINE_PROTOCOL_DIR := build/limine-protocol
# Pin both halves of the boot interface; do not silently follow moving branches.
LIMINE_COMMIT := 5be26a73d7b7b4d4477d18be94e1d16e615adf56
LIMINE_PROTOCOL_COMMIT := 3a0526b700e356f0eac1b71a77697b3fd1c707a3
HOST_EXE :=
ifneq ($(filter Windows_NT,$(OS)),)
HOST_EXE := .exe
endif
LIMINE_TOOL := $(LIMINE_DIR)/limine$(HOST_EXE)

ZIG_VERSION := 0.15.2
ZIG_DIR := build/tooling/zig
ZIG := $(ZIG_DIR)/zig$(HOST_EXE)
CC := $(ZIG) cc
CXX := $(ZIG) c++
AR := $(ZIG) ar
LD := $(ZIG) ld.lld
QEMU := qemu-system-x86_64
SMOKE_BOOT_WAIT ?= 180
QEMU_A1466 := build/tooling/qemu-a1466-src/build/qemu-system-x86_64$(HOST_EXE)
A1466_SSDT := build/a1466-input.aml
PYTHON ?= python3
HOST_CC ?= cc
HOST_AR ?= ar
HOST_NM ?= nm

CFLAGS := \
	-target x86_64-freestanding-none \
	-std=gnu11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-lto \
	-fno-PIC \
	-ffunction-sections \
	-fdata-sections \
	-m64 \
	-march=x86_64 \
	-mabi=sysv \
	-mno-80387 \
	-mno-mmx \
	-mno-red-zone \
	-mcmodel=kernel \
	-Wall \
	-Wextra \
	-Werror \
	-g \
	-O2 \
	-I shared/include \
	-I kernel/include \
	-I $(LIMINE_PROTOCOL_DIR)/include \
	-MMD \
	-MP

LDFLAGS := \
	-m elf_x86_64 \
	-nostdlib \
	-static \
	-z max-page-size=0x1000 \
	--gc-sections \
	-T kernel/linker.ld

USER_CFLAGS := \
	-target x86_64-freestanding-none \
	-std=gnu11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-lto \
	-fno-PIC \
	-ffunction-sections \
	-fdata-sections \
	-m64 \
	-march=x86_64 \
	-mabi=sysv \
	-mno-80387 \
	-mno-mmx \
	-mno-red-zone \
	-Wall \
	-Wextra \
	-Werror \
	-g \
	-O2 \
	-I shared/include \
	-I userspace/lib/include \
	-MMD \
	-MP

USER_CXXFLAGS := \
	-target x86_64-freestanding-none \
	-std=gnu++20 \
	-ffreestanding \
	-fno-exceptions \
	-fno-rtti \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-lto \
	-fno-PIC \
	-ffunction-sections \
	-fdata-sections \
	-m64 \
	-march=x86_64 \
	-mabi=sysv \
	-mno-80387 \
	-mno-mmx \
	-mno-red-zone \
	-Wall \
	-Wextra \
	-Werror \
	-g \
	-O2 \
	-I shared/include \
	-I userspace/lib/include \
	-MMD \
	-MP

USER_STL_CXXFLAGS := \
	-target x86_64-freestanding-none \
	-std=gnu++20 \
	-ffreestanding \
	-fno-exceptions \
	-fno-rtti \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-lto \
	-fno-PIC \
	-ffunction-sections \
	-fdata-sections \
	-m64 \
	-march=x86_64 \
	-mabi=sysv \
	-mno-80387 \
	-mno-mmx \
	-mno-red-zone \
	-Wall \
	-Wextra \
	-Werror \
	-g \
	-O2 \
	-MMD \
	-MP \
	-nostdinc++ \
	-fno-strict-aliasing \
	-Wno-unused-parameter \
	-Wno-deprecated-declarations \
	-Wno-invalid-offsetof \
	-include ports/srvros/node/srvros-node-probe-shims.h \
	-D_LIBCPP_HAS_NO_THREADS \
	-D_LIBCPP_HAS_MONOTONIC_CLOCK=1 \
	-D_LIBCPP_HAS_CLOCK_GETTIME=1 \
	-D_LIBCPP_HAS_WIDE_CHARACTERS=1 \
	-D_LIBCPP_HAS_LOCALIZATION=1 \
	-D_LIBCPP_HAS_FILESYSTEM=1 \
	-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE=1 \
	-D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE \
	-DPATH_MAX=1024 \
	-DHOST_NAME_MAX=63 \
	-I $(ZIG_DIR)/lib/libcxx/include \
	-I $(ZIG_DIR)/lib/libcxx/src/include \
	-I $(ZIG_DIR)/lib/libcxxabi/include \
	-I $(ZIG_DIR)/lib/include \
	-isystem shared/include \
	-isystem userspace/lib/include

USER_LDFLAGS := \
	-m elf_x86_64 \
	-nostdlib \
	-static \
	-z max-page-size=0x1000 \
	--gc-sections \
	-T userspace/linker.ld

USER_APP_LDFLAGS := \
	-m elf_x86_64 \
	-nostdlib \
	-static \
	-z max-page-size=0x1000 \
	--gc-sections \
	-T userspace/app.ld

USER_CRT0_OBJ := build/userspace/lib/crt0.S.o
USER_LIB_A := build/userspace/lib/libsrvros.a
USER_SYSROOT := build/sysroot/srvros
USER_SYSROOT_HEADERS := $(shell find shared/include userspace/lib/include -type f 2>/dev/null | LC_ALL=C sort)
KERNEL_HEADERS := $(shell find shared/include kernel/include -type f 2>/dev/null | LC_ALL=C sort)

KERNEL_C := $(shell find kernel/src -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
KERNEL_S := $(shell find kernel/src -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
UACPI_KERNEL_SRC := $(wildcard ports/upstream/uacpi/source/*.c) ports/srvros/uacpi/kernel/probe.c \
    ports/srvros/uacpi/runtime/work_queue.c ports/srvros/uacpi/runtime/work_worker.c \
    ports/srvros/uacpi/runtime/work_worker_test.c ports/srvros/uacpi/runtime/sci_test.c
UACPI_KERNEL_OBJ := $(UACPI_KERNEL_SRC:%.c=build/kernel/%.c.o)
KERNEL_OBJ := $(KERNEL_C:%.c=build/%.c.o) $(KERNEL_S:%.S=build/%.S.o) $(UACPI_KERNEL_OBJ)
KERNEL_DEP := $(KERNEL_C:%.c=build/%.c.d) $(KERNEL_S:%.S=build/%.S.d) $(UACPI_KERNEL_OBJ:.o=.d)

USER_INIT_C := $(shell find userspace/init -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_INIT_S := $(shell find userspace/init -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_INIT_OBJ := $(USER_INIT_C:%.c=build/%.c.o) $(USER_INIT_S:%.S=build/%.S.o)
USER_INIT_DEP := $(USER_INIT_C:%.c=build/%.c.d) $(USER_INIT_S:%.S=build/%.S.d)

USER_HELLO_C := $(shell find userspace/hello -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_HELLO_S := $(shell find userspace/hello -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_HELLO_OBJ := $(USER_HELLO_C:%.c=build/%.c.o) $(USER_HELLO_S:%.S=build/%.S.o)
USER_HELLO_DEP := $(USER_HELLO_C:%.c=build/%.c.d) $(USER_HELLO_S:%.S=build/%.S.d)

USER_CAT_C := $(shell find userspace/cat -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_CAT_S := $(shell find userspace/cat -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_CAT_OBJ := $(USER_CAT_C:%.c=build/%.c.o) $(USER_CAT_S:%.S=build/%.S.o)
USER_CAT_DEP := $(USER_CAT_C:%.c=build/%.c.d) $(USER_CAT_S:%.S=build/%.S.d)

USER_MORE_C := $(shell find userspace/more -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_MORE_S := $(shell find userspace/more -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_MORE_OBJ := $(USER_MORE_C:%.c=build/%.c.o) $(USER_MORE_S:%.S=build/%.S.o)
USER_MORE_DEP := $(USER_MORE_C:%.c=build/%.c.d) $(USER_MORE_S:%.S=build/%.S.d)

USER_SH_C := $(shell find userspace/sh -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SH_S := $(shell find userspace/sh -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SH_OBJ := $(USER_SH_C:%.c=build/%.c.o) $(USER_SH_S:%.S=build/%.S.o)
USER_SH_DEP := $(USER_SH_C:%.c=build/%.c.d) $(USER_SH_S:%.S=build/%.S.d)

USER_LS_C := $(shell find userspace/ls -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_LS_S := $(shell find userspace/ls -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_LS_OBJ := $(USER_LS_C:%.c=build/%.c.o) $(USER_LS_S:%.S=build/%.S.o)
USER_LS_DEP := $(USER_LS_C:%.c=build/%.c.d) $(USER_LS_S:%.S=build/%.S.d)

USER_ECHO_C := $(shell find userspace/echo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_ECHO_S := $(shell find userspace/echo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_ECHO_OBJ := $(USER_ECHO_C:%.c=build/%.c.o) $(USER_ECHO_S:%.S=build/%.S.o)
USER_ECHO_DEP := $(USER_ECHO_C:%.c=build/%.c.d) $(USER_ECHO_S:%.S=build/%.S.d)

USER_WRITE_C := $(shell find userspace/write -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_WRITE_S := $(shell find userspace/write -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_WRITE_OBJ := $(USER_WRITE_C:%.c=build/%.c.o) $(USER_WRITE_S:%.S=build/%.S.o)
USER_WRITE_DEP := $(USER_WRITE_C:%.c=build/%.c.d) $(USER_WRITE_S:%.S=build/%.S.d)

USER_WC_C := $(shell find userspace/wc -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_WC_S := $(shell find userspace/wc -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_WC_OBJ := $(USER_WC_C:%.c=build/%.c.o) $(USER_WC_S:%.S=build/%.S.o)
USER_WC_DEP := $(USER_WC_C:%.c=build/%.c.d) $(USER_WC_S:%.S=build/%.S.d)

USER_CLEAR_C := $(shell find userspace/clear -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_CLEAR_S := $(shell find userspace/clear -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_CLEAR_OBJ := $(USER_CLEAR_C:%.c=build/%.c.o) $(USER_CLEAR_S:%.S=build/%.S.o)
USER_CLEAR_DEP := $(USER_CLEAR_C:%.c=build/%.c.d) $(USER_CLEAR_S:%.S=build/%.S.d)

USER_PS_C := $(shell find userspace/ps -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_PS_S := $(shell find userspace/ps -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_PS_OBJ := $(USER_PS_C:%.c=build/%.c.o) $(USER_PS_S:%.S=build/%.S.o)
USER_PS_DEP := $(USER_PS_C:%.c=build/%.c.d) $(USER_PS_S:%.S=build/%.S.d)

USER_KILL_C := $(shell find userspace/kill -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_KILL_S := $(shell find userspace/kill -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_KILL_OBJ := $(USER_KILL_C:%.c=build/%.c.o) $(USER_KILL_S:%.S=build/%.S.o)
USER_KILL_DEP := $(USER_KILL_C:%.c=build/%.c.d) $(USER_KILL_S:%.S=build/%.S.d)

USER_WHICH_C := $(shell find userspace/which -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_WHICH_S := $(shell find userspace/which -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_WHICH_OBJ := $(USER_WHICH_C:%.c=build/%.c.o) $(USER_WHICH_S:%.S=build/%.S.o)
USER_WHICH_DEP := $(USER_WHICH_C:%.c=build/%.c.d) $(USER_WHICH_S:%.S=build/%.S.d)

USER_ENV_C := $(shell find userspace/env -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_ENV_S := $(shell find userspace/env -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_ENV_OBJ := $(USER_ENV_C:%.c=build/%.c.o) $(USER_ENV_S:%.S=build/%.S.o)
USER_ENV_DEP := $(USER_ENV_C:%.c=build/%.c.d) $(USER_ENV_S:%.S=build/%.S.d)

USER_PWD_C := $(shell find userspace/pwd -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_PWD_S := $(shell find userspace/pwd -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_PWD_OBJ := $(USER_PWD_C:%.c=build/%.c.o) $(USER_PWD_S:%.S=build/%.S.o)
USER_PWD_DEP := $(USER_PWD_C:%.c=build/%.c.d) $(USER_PWD_S:%.S=build/%.S.d)

USER_TRUE_C := $(shell find userspace/true -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TRUE_S := $(shell find userspace/true -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TRUE_OBJ := $(USER_TRUE_C:%.c=build/%.c.o) $(USER_TRUE_S:%.S=build/%.S.o)
USER_TRUE_DEP := $(USER_TRUE_C:%.c=build/%.c.d) $(USER_TRUE_S:%.S=build/%.S.d)

USER_FALSE_C := $(shell find userspace/false -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_FALSE_S := $(shell find userspace/false -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_FALSE_OBJ := $(USER_FALSE_C:%.c=build/%.c.o) $(USER_FALSE_S:%.S=build/%.S.o)
USER_FALSE_DEP := $(USER_FALSE_C:%.c=build/%.c.d) $(USER_FALSE_S:%.S=build/%.S.d)

USER_SLEEP_C := $(shell find userspace/sleep -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SLEEP_S := $(shell find userspace/sleep -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SLEEP_OBJ := $(USER_SLEEP_C:%.c=build/%.c.o) $(USER_SLEEP_S:%.S=build/%.S.o)
USER_SLEEP_DEP := $(USER_SLEEP_C:%.c=build/%.c.d) $(USER_SLEEP_S:%.S=build/%.S.d)

USER_DATE_C := $(shell find userspace/date -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_DATE_S := $(shell find userspace/date -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_DATE_OBJ := $(USER_DATE_C:%.c=build/%.c.o) $(USER_DATE_S:%.S=build/%.S.o)
USER_DATE_DEP := $(USER_DATE_C:%.c=build/%.c.d) $(USER_DATE_S:%.S=build/%.S.d)

USER_TOUCH_C := $(shell find userspace/touch -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TOUCH_S := $(shell find userspace/touch -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TOUCH_OBJ := $(USER_TOUCH_C:%.c=build/%.c.o) $(USER_TOUCH_S:%.S=build/%.S.o)
USER_TOUCH_DEP := $(USER_TOUCH_C:%.c=build/%.c.d) $(USER_TOUCH_S:%.S=build/%.S.d)

USER_MKTEMP_C := $(shell find userspace/mktemp -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_MKTEMP_S := $(shell find userspace/mktemp -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_MKTEMP_OBJ := $(USER_MKTEMP_C:%.c=build/%.c.o) $(USER_MKTEMP_S:%.S=build/%.S.o)
USER_MKTEMP_DEP := $(USER_MKTEMP_C:%.c=build/%.c.d) $(USER_MKTEMP_S:%.S=build/%.S.d)

USER_BASENAME_C := $(shell find userspace/basename -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_BASENAME_S := $(shell find userspace/basename -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_BASENAME_OBJ := $(USER_BASENAME_C:%.c=build/%.c.o) $(USER_BASENAME_S:%.S=build/%.S.o)
USER_BASENAME_DEP := $(USER_BASENAME_C:%.c=build/%.c.d) $(USER_BASENAME_S:%.S=build/%.S.d)

USER_DIRNAME_C := $(shell find userspace/dirname -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_DIRNAME_S := $(shell find userspace/dirname -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_DIRNAME_OBJ := $(USER_DIRNAME_C:%.c=build/%.c.o) $(USER_DIRNAME_S:%.S=build/%.S.o)
USER_DIRNAME_DEP := $(USER_DIRNAME_C:%.c=build/%.c.d) $(USER_DIRNAME_S:%.S=build/%.S.d)

USER_GREP_C := $(shell find userspace/grep -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_GREP_S := $(shell find userspace/grep -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_GREP_OBJ := $(USER_GREP_C:%.c=build/%.c.o) $(USER_GREP_S:%.S=build/%.S.o)
USER_GREP_DEP := $(USER_GREP_C:%.c=build/%.c.d) $(USER_GREP_S:%.S=build/%.S.d)

USER_HEAD_C := $(shell find userspace/head -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_HEAD_S := $(shell find userspace/head -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_HEAD_OBJ := $(USER_HEAD_C:%.c=build/%.c.o) $(USER_HEAD_S:%.S=build/%.S.o)
USER_HEAD_DEP := $(USER_HEAD_C:%.c=build/%.c.d) $(USER_HEAD_S:%.S=build/%.S.d)

USER_TAIL_C := $(shell find userspace/tail -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TAIL_S := $(shell find userspace/tail -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TAIL_OBJ := $(USER_TAIL_C:%.c=build/%.c.o) $(USER_TAIL_S:%.S=build/%.S.o)
USER_TAIL_DEP := $(USER_TAIL_C:%.c=build/%.c.d) $(USER_TAIL_S:%.S=build/%.S.d)

USER_TEE_C := $(shell find userspace/tee -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TEE_S := $(shell find userspace/tee -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TEE_OBJ := $(USER_TEE_C:%.c=build/%.c.o) $(USER_TEE_S:%.S=build/%.S.o)
USER_TEE_DEP := $(USER_TEE_C:%.c=build/%.c.d) $(USER_TEE_S:%.S=build/%.S.d)

USER_UNAME_C := $(shell find userspace/uname -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_UNAME_S := $(shell find userspace/uname -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_UNAME_OBJ := $(USER_UNAME_C:%.c=build/%.c.o) $(USER_UNAME_S:%.S=build/%.S.o)
USER_UNAME_DEP := $(USER_UNAME_C:%.c=build/%.c.d) $(USER_UNAME_S:%.S=build/%.S.d)

USER_HOSTNAME_C := $(shell find userspace/hostname -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_HOSTNAME_S := $(shell find userspace/hostname -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_HOSTNAME_OBJ := $(USER_HOSTNAME_C:%.c=build/%.c.o) $(USER_HOSTNAME_S:%.S=build/%.S.o)
USER_HOSTNAME_DEP := $(USER_HOSTNAME_C:%.c=build/%.c.d) $(USER_HOSTNAME_S:%.S=build/%.S.d)

USER_UPTIME_C := $(shell find userspace/uptime -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_UPTIME_S := $(shell find userspace/uptime -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_UPTIME_OBJ := $(USER_UPTIME_C:%.c=build/%.c.o) $(USER_UPTIME_S:%.S=build/%.S.o)
USER_UPTIME_DEP := $(USER_UPTIME_C:%.c=build/%.c.d) $(USER_UPTIME_S:%.S=build/%.S.d)

USER_FIND_C := $(shell find userspace/find -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_FIND_S := $(shell find userspace/find -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_FIND_OBJ := $(USER_FIND_C:%.c=build/%.c.o) $(USER_FIND_S:%.S=build/%.S.o)
USER_FIND_DEP := $(USER_FIND_C:%.c=build/%.c.d) $(USER_FIND_S:%.S=build/%.S.d)

USER_DU_C := $(shell find userspace/du -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_DU_S := $(shell find userspace/du -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_DU_OBJ := $(USER_DU_C:%.c=build/%.c.o) $(USER_DU_S:%.S=build/%.S.o)
USER_DU_DEP := $(USER_DU_C:%.c=build/%.c.d) $(USER_DU_S:%.S=build/%.S.d)

USER_DF_C := $(shell find userspace/df -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_DF_S := $(shell find userspace/df -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_DF_OBJ := $(USER_DF_C:%.c=build/%.c.o) $(USER_DF_S:%.S=build/%.S.o)
USER_DF_DEP := $(USER_DF_C:%.c=build/%.c.d) $(USER_DF_S:%.S=build/%.S.d)

USER_SORT_C := $(shell find userspace/sort -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SORT_S := $(shell find userspace/sort -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SORT_OBJ := $(USER_SORT_C:%.c=build/%.c.o) $(USER_SORT_S:%.S=build/%.S.o)
USER_SORT_DEP := $(USER_SORT_C:%.c=build/%.c.d) $(USER_SORT_S:%.S=build/%.S.d)

USER_UNIQ_C := $(shell find userspace/uniq -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_UNIQ_S := $(shell find userspace/uniq -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_UNIQ_OBJ := $(USER_UNIQ_C:%.c=build/%.c.o) $(USER_UNIQ_S:%.S=build/%.S.o)
USER_UNIQ_DEP := $(USER_UNIQ_C:%.c=build/%.c.d) $(USER_UNIQ_S:%.S=build/%.S.d)

USER_CUT_C := $(shell find userspace/cut -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_CUT_S := $(shell find userspace/cut -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_CUT_OBJ := $(USER_CUT_C:%.c=build/%.c.o) $(USER_CUT_S:%.S=build/%.S.o)
USER_CUT_DEP := $(USER_CUT_C:%.c=build/%.c.d) $(USER_CUT_S:%.S=build/%.S.d)

USER_XARGS_C := $(shell find userspace/xargs -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_XARGS_S := $(shell find userspace/xargs -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_XARGS_OBJ := $(USER_XARGS_C:%.c=build/%.c.o) $(USER_XARGS_S:%.S=build/%.S.o)
USER_XARGS_DEP := $(USER_XARGS_C:%.c=build/%.c.d) $(USER_XARGS_S:%.S=build/%.S.d)

USER_SEQ_C := $(shell find userspace/seq -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SEQ_S := $(shell find userspace/seq -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SEQ_OBJ := $(USER_SEQ_C:%.c=build/%.c.o) $(USER_SEQ_S:%.S=build/%.S.o)
USER_SEQ_DEP := $(USER_SEQ_C:%.c=build/%.c.d) $(USER_SEQ_S:%.S=build/%.S.d)

USER_REALPATH_C := $(shell find userspace/realpath -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_REALPATH_S := $(shell find userspace/realpath -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_REALPATH_OBJ := $(USER_REALPATH_C:%.c=build/%.c.o) $(USER_REALPATH_S:%.S=build/%.S.o)
USER_REALPATH_DEP := $(USER_REALPATH_C:%.c=build/%.c.d) $(USER_REALPATH_S:%.S=build/%.S.d)

USER_ID_C := $(shell find userspace/id -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_ID_S := $(shell find userspace/id -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_ID_OBJ := $(USER_ID_C:%.c=build/%.c.o) $(USER_ID_S:%.S=build/%.S.o)
USER_ID_DEP := $(USER_ID_C:%.c=build/%.c.d) $(USER_ID_S:%.S=build/%.S.d)

USER_WHOAMI_C := $(shell find userspace/whoami -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_WHOAMI_S := $(shell find userspace/whoami -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_WHOAMI_OBJ := $(USER_WHOAMI_C:%.c=build/%.c.o) $(USER_WHOAMI_S:%.S=build/%.S.o)
USER_WHOAMI_DEP := $(USER_WHOAMI_C:%.c=build/%.c.d) $(USER_WHOAMI_S:%.S=build/%.S.d)

USER_READLINK_C := $(shell find userspace/readlink -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_READLINK_S := $(shell find userspace/readlink -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_READLINK_OBJ := $(USER_READLINK_C:%.c=build/%.c.o) $(USER_READLINK_S:%.S=build/%.S.o)
USER_READLINK_DEP := $(USER_READLINK_C:%.c=build/%.c.d) $(USER_READLINK_S:%.S=build/%.S.d)

USER_CMP_C := $(shell find userspace/cmp -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_CMP_S := $(shell find userspace/cmp -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_CMP_OBJ := $(USER_CMP_C:%.c=build/%.c.o) $(USER_CMP_S:%.S=build/%.S.o)
USER_CMP_DEP := $(USER_CMP_C:%.c=build/%.c.d) $(USER_CMP_S:%.S=build/%.S.d)

USER_YES_C := $(shell find userspace/yes -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_YES_S := $(shell find userspace/yes -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_YES_OBJ := $(USER_YES_C:%.c=build/%.c.o) $(USER_YES_S:%.S=build/%.S.o)
USER_YES_DEP := $(USER_YES_C:%.c=build/%.c.d) $(USER_YES_S:%.S=build/%.S.d)

USER_INSTALL_C := $(shell find userspace/install -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_INSTALL_S := $(shell find userspace/install -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_INSTALL_OBJ := $(USER_INSTALL_C:%.c=build/%.c.o) $(USER_INSTALL_S:%.S=build/%.S.o)
USER_INSTALL_DEP := $(USER_INSTALL_C:%.c=build/%.c.d) $(USER_INSTALL_S:%.S=build/%.S.d)

USER_DIFF_C := $(shell find userspace/diff -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_DIFF_S := $(shell find userspace/diff -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_DIFF_OBJ := $(USER_DIFF_C:%.c=build/%.c.o) $(USER_DIFF_S:%.S=build/%.S.o)
USER_DIFF_DEP := $(USER_DIFF_C:%.c=build/%.c.d) $(USER_DIFF_S:%.S=build/%.S.d)

USER_TAR_C := $(shell find userspace/tar -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TAR_S := $(shell find userspace/tar -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TAR_OBJ := $(USER_TAR_C:%.c=build/%.c.o) $(USER_TAR_S:%.S=build/%.S.o)
USER_TAR_DEP := $(USER_TAR_C:%.c=build/%.c.d) $(USER_TAR_S:%.S=build/%.S.d)

USER_GZIP_C := $(shell find userspace/gzip -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_GZIP_S := $(shell find userspace/gzip -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_GZIP_OBJ := $(USER_GZIP_C:%.c=build/%.c.o) $(USER_GZIP_S:%.S=build/%.S.o)
USER_GZIP_DEP := $(USER_GZIP_C:%.c=build/%.c.d) $(USER_GZIP_S:%.S=build/%.S.d)

USER_PATCH_C := $(shell find userspace/patch -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_PATCH_S := $(shell find userspace/patch -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_PATCH_OBJ := $(USER_PATCH_C:%.c=build/%.c.o) $(USER_PATCH_S:%.S=build/%.S.o)
USER_PATCH_DEP := $(USER_PATCH_C:%.c=build/%.c.d) $(USER_PATCH_S:%.S=build/%.S.d)

USER_MAKE_C := $(shell find userspace/make -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_MAKE_S := $(shell find userspace/make -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_MAKE_OBJ := $(USER_MAKE_C:%.c=build/%.c.o) $(USER_MAKE_S:%.S=build/%.S.o)
USER_MAKE_DEP := $(USER_MAKE_C:%.c=build/%.c.d) $(USER_MAKE_S:%.S=build/%.S.d)

USER_SED_C := $(shell find userspace/sed -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SED_S := $(shell find userspace/sed -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SED_OBJ := $(USER_SED_C:%.c=build/%.c.o) $(USER_SED_S:%.S=build/%.S.o)
USER_SED_DEP := $(USER_SED_C:%.c=build/%.c.d) $(USER_SED_S:%.S=build/%.S.d)

USER_EXPR_C := $(shell find userspace/expr -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_EXPR_S := $(shell find userspace/expr -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_EXPR_OBJ := $(USER_EXPR_C:%.c=build/%.c.o) $(USER_EXPR_S:%.S=build/%.S.o)
USER_EXPR_DEP := $(USER_EXPR_C:%.c=build/%.c.d) $(USER_EXPR_S:%.S=build/%.S.d)

USER_PRINTF_C := $(shell find userspace/printf -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_PRINTF_S := $(shell find userspace/printf -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_PRINTF_OBJ := $(USER_PRINTF_C:%.c=build/%.c.o) $(USER_PRINTF_S:%.S=build/%.S.o)
USER_PRINTF_DEP := $(USER_PRINTF_C:%.c=build/%.c.d) $(USER_PRINTF_S:%.S=build/%.S.d)

USER_TR_C := $(shell find userspace/tr -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TR_S := $(shell find userspace/tr -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TR_OBJ := $(USER_TR_C:%.c=build/%.c.o) $(USER_TR_S:%.S=build/%.S.o)
USER_TR_DEP := $(USER_TR_C:%.c=build/%.c.d) $(USER_TR_S:%.S=build/%.S.d)

USER_DD_C := $(shell find userspace/dd -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_DD_S := $(shell find userspace/dd -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_DD_OBJ := $(USER_DD_C:%.c=build/%.c.o) $(USER_DD_S:%.S=build/%.S.o)
USER_DD_DEP := $(USER_DD_C:%.c=build/%.c.d) $(USER_DD_S:%.S=build/%.S.d)

USER_ED_C := $(shell find userspace/ed -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_ED_S := $(shell find userspace/ed -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_ED_OBJ := $(USER_ED_C:%.c=build/%.c.o) $(USER_ED_S:%.S=build/%.S.o)
USER_ED_DEP := $(USER_ED_C:%.c=build/%.c.d) $(USER_ED_S:%.S=build/%.S.d)

USER_POSIXUTILS_C := $(shell find userspace/posixutils -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_POSIXUTILS_S := $(shell find userspace/posixutils -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_POSIXUTILS_OBJ := $(USER_POSIXUTILS_C:%.c=build/%.c.o) $(USER_POSIXUTILS_S:%.S=build/%.S.o)
USER_POSIXUTILS_DEP := $(USER_POSIXUTILS_C:%.c=build/%.c.d) $(USER_POSIXUTILS_S:%.S=build/%.S.d)

USER_STAT_C := $(shell find userspace/stat -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_STAT_S := $(shell find userspace/stat -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_STAT_OBJ := $(USER_STAT_C:%.c=build/%.c.o) $(USER_STAT_S:%.S=build/%.S.o)
USER_STAT_DEP := $(USER_STAT_C:%.c=build/%.c.d) $(USER_STAT_S:%.S=build/%.S.d)

USER_CHMOD_C := $(shell find userspace/chmod -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_CHMOD_S := $(shell find userspace/chmod -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_CHMOD_OBJ := $(USER_CHMOD_C:%.c=build/%.c.o) $(USER_CHMOD_S:%.S=build/%.S.o)
USER_CHMOD_DEP := $(USER_CHMOD_C:%.c=build/%.c.d) $(USER_CHMOD_S:%.S=build/%.S.d)

USER_CP_C := $(shell find userspace/cp -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_CP_S := $(shell find userspace/cp -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_CP_OBJ := $(USER_CP_C:%.c=build/%.c.o) $(USER_CP_S:%.S=build/%.S.o)
USER_CP_DEP := $(USER_CP_C:%.c=build/%.c.d) $(USER_CP_S:%.S=build/%.S.d)

USER_RM_C := $(shell find userspace/rm -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_RM_S := $(shell find userspace/rm -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_RM_OBJ := $(USER_RM_C:%.c=build/%.c.o) $(USER_RM_S:%.S=build/%.S.o)
USER_RM_DEP := $(USER_RM_C:%.c=build/%.c.d) $(USER_RM_S:%.S=build/%.S.d)

USER_MKDIR_C := $(shell find userspace/mkdir -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_MKDIR_S := $(shell find userspace/mkdir -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_MKDIR_OBJ := $(USER_MKDIR_C:%.c=build/%.c.o) $(USER_MKDIR_S:%.S=build/%.S.o)
USER_MKDIR_DEP := $(USER_MKDIR_C:%.c=build/%.c.d) $(USER_MKDIR_S:%.S=build/%.S.d)

USER_MV_C := $(shell find userspace/mv -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_MV_S := $(shell find userspace/mv -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_MV_OBJ := $(USER_MV_C:%.c=build/%.c.o) $(USER_MV_S:%.S=build/%.S.o)
USER_MV_DEP := $(USER_MV_C:%.c=build/%.c.d) $(USER_MV_S:%.S=build/%.S.d)

USER_TAP_C := $(shell find userspace/tap -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TAP_S := $(shell find userspace/tap -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TAP_OBJ := $(USER_TAP_C:%.c=build/%.c.o) $(USER_TAP_S:%.S=build/%.S.o)
USER_TAP_DEP := $(USER_TAP_C:%.c=build/%.c.d) $(USER_TAP_S:%.S=build/%.S.d)

USER_SVSCAN_C := $(shell find userspace/svscan -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SVSCAN_S := $(shell find userspace/svscan -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SVSCAN_OBJ := $(USER_SVSCAN_C:%.c=build/%.c.o) $(USER_SVSCAN_S:%.S=build/%.S.o)
USER_SVSCAN_DEP := $(USER_SVSCAN_C:%.c=build/%.c.d) $(USER_SVSCAN_S:%.S=build/%.S.d)

USER_WEBD_C := $(shell find userspace/webd -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_WEBD_S := $(shell find userspace/webd -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_WEBD_OBJ := $(USER_WEBD_C:%.c=build/%.c.o) $(USER_WEBD_S:%.S=build/%.S.o)
USER_WEBD_DEP := $(USER_WEBD_C:%.c=build/%.c.d) $(USER_WEBD_S:%.S=build/%.S.d)

USER_HTTPGET_C := $(shell find userspace/httpget -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_HTTPGET_S := $(shell find userspace/httpget -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_HTTPGET_OBJ := $(USER_HTTPGET_C:%.c=build/%.c.o) $(USER_HTTPGET_S:%.S=build/%.S.o)
USER_HTTPGET_DEP := $(USER_HTTPGET_C:%.c=build/%.c.d) $(USER_HTTPGET_S:%.S=build/%.S.d)

USER_UDPDNS_C := $(shell find userspace/udpdns -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_UDPDNS_S := $(shell find userspace/udpdns -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_UDPDNS_OBJ := $(USER_UDPDNS_C:%.c=build/%.c.o) $(USER_UDPDNS_S:%.S=build/%.S.o)
USER_UDPDNS_DEP := $(USER_UDPDNS_C:%.c=build/%.c.d) $(USER_UDPDNS_S:%.S=build/%.S.d)

USER_UDPECHO_C := $(shell find userspace/udpecho -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_UDPECHO_S := $(shell find userspace/udpecho -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_UDPECHO_OBJ := $(USER_UDPECHO_C:%.c=build/%.c.o) $(USER_UDPECHO_S:%.S=build/%.S.o)
USER_UDPECHO_DEP := $(USER_UDPECHO_C:%.c=build/%.c.d) $(USER_UDPECHO_S:%.S=build/%.S.d)

USER_NETSTAT_C := $(shell find userspace/netstat -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_NETSTAT_S := $(shell find userspace/netstat -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_NETSTAT_OBJ := $(USER_NETSTAT_C:%.c=build/%.c.o) $(USER_NETSTAT_S:%.S=build/%.S.o)
USER_NETSTAT_DEP := $(USER_NETSTAT_C:%.c=build/%.c.d) $(USER_NETSTAT_S:%.S=build/%.S.d)

USER_IFCONFIG_C := $(shell find userspace/ifconfig -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_IFCONFIG_S := $(shell find userspace/ifconfig -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_IFCONFIG_OBJ := $(USER_IFCONFIG_C:%.c=build/%.c.o) $(USER_IFCONFIG_S:%.S=build/%.S.o)
USER_IFCONFIG_DEP := $(USER_IFCONFIG_C:%.c=build/%.c.d) $(USER_IFCONFIG_S:%.S=build/%.S.d)

USER_ROUTE_C := $(shell find userspace/route -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_ROUTE_S := $(shell find userspace/route -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_ROUTE_OBJ := $(USER_ROUTE_C:%.c=build/%.c.o) $(USER_ROUTE_S:%.S=build/%.S.o)
USER_ROUTE_DEP := $(USER_ROUTE_C:%.c=build/%.c.d) $(USER_ROUTE_S:%.S=build/%.S.d)

USER_ARP_C := $(shell find userspace/arp -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_ARP_S := $(shell find userspace/arp -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_ARP_OBJ := $(USER_ARP_C:%.c=build/%.c.o) $(USER_ARP_S:%.S=build/%.S.o)
USER_ARP_DEP := $(USER_ARP_C:%.c=build/%.c.d) $(USER_ARP_S:%.S=build/%.S.d)

USER_PING_C := $(shell find userspace/ping -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_PING_S := $(shell find userspace/ping -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_PING_OBJ := $(USER_PING_C:%.c=build/%.c.o) $(USER_PING_S:%.S=build/%.S.o)
USER_PING_DEP := $(USER_PING_C:%.c=build/%.c.d) $(USER_PING_S:%.S=build/%.S.d)

USER_HOST_C := $(shell find userspace/host -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_HOST_S := $(shell find userspace/host -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_HOST_OBJ := $(USER_HOST_C:%.c=build/%.c.o) $(USER_HOST_S:%.S=build/%.S.o)
USER_HOST_DEP := $(USER_HOST_C:%.c=build/%.c.d) $(USER_HOST_S:%.S=build/%.S.d)

USER_NETCHECK_C := $(shell find userspace/netcheck -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_NETCHECK_S := $(shell find userspace/netcheck -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_NETCHECK_OBJ := $(USER_NETCHECK_C:%.c=build/%.c.o) $(USER_NETCHECK_S:%.S=build/%.S.o)
USER_NETCHECK_DEP := $(USER_NETCHECK_C:%.c=build/%.c.d) $(USER_NETCHECK_S:%.S=build/%.S.d)

USER_NETABI_C := $(shell find userspace/netabi -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_NETABI_S := $(shell find userspace/netabi -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_NETABI_OBJ := $(USER_NETABI_C:%.c=build/%.c.o) $(USER_NETABI_S:%.S=build/%.S.o)
USER_NETABI_DEP := $(USER_NETABI_C:%.c=build/%.c.d) $(USER_NETABI_S:%.S=build/%.S.d)

USER_TCPSTRESS_C := $(shell find userspace/tcpstress -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TCPSTRESS_S := $(shell find userspace/tcpstress -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TCPSTRESS_OBJ := $(USER_TCPSTRESS_C:%.c=build/%.c.o) $(USER_TCPSTRESS_S:%.S=build/%.S.o)
USER_TCPSTRESS_DEP := $(USER_TCPSTRESS_C:%.c=build/%.c.d) $(USER_TCPSTRESS_S:%.S=build/%.S.d)

USER_TCPPROBE_C := $(shell find userspace/tcpprobe -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TCPPROBE_S := $(shell find userspace/tcpprobe -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TCPPROBE_OBJ := $(USER_TCPPROBE_C:%.c=build/%.c.o) $(USER_TCPPROBE_S:%.S=build/%.S.o)
USER_TCPPROBE_DEP := $(USER_TCPPROBE_C:%.c=build/%.c.d) $(USER_TCPPROBE_S:%.S=build/%.S.d)

USER_SYSABI_C := $(shell find userspace/sysabi -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SYSABI_S := $(shell find userspace/sysabi -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SYSABI_OBJ := $(USER_SYSABI_C:%.c=build/%.c.o) $(USER_SYSABI_S:%.S=build/%.S.o)
USER_SYSABI_DEP := $(USER_SYSABI_C:%.c=build/%.c.d) $(USER_SYSABI_S:%.S=build/%.S.d)

USER_SPIN_C := $(shell find userspace/spin -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SPIN_S := $(shell find userspace/spin -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SPIN_OBJ := $(USER_SPIN_C:%.c=build/%.c.o) $(USER_SPIN_S:%.S=build/%.S.o)
USER_SPIN_DEP := $(USER_SPIN_C:%.c=build/%.c.d) $(USER_SPIN_S:%.S=build/%.S.d)

USER_FPDEMO_C := $(shell find userspace/fpdemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_FPDEMO_S := $(shell find userspace/fpdemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_FPDEMO_OBJ := $(USER_FPDEMO_C:%.c=build/%.c.o) $(USER_FPDEMO_S:%.S=build/%.S.o)
USER_FPDEMO_DEP := $(USER_FPDEMO_C:%.c=build/%.c.d) $(USER_FPDEMO_S:%.S=build/%.S.d)

USER_UI_C := $(shell find userspace/ui -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_UI_S := $(shell find userspace/ui -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_UI_OBJ := $(USER_UI_C:%.c=build/%.c.o) $(USER_UI_S:%.S=build/%.S.o)
USER_UI_DEP := $(USER_UI_C:%.c=build/%.c.d) $(USER_UI_S:%.S=build/%.S.d)

USER_DESKTOP_C := $(shell find userspace/desktop -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_DESKTOP_S := $(shell find userspace/desktop -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_DESKTOP_OBJ := $(USER_DESKTOP_C:%.c=build/%.c.o) $(USER_DESKTOP_S:%.S=build/%.S.o)
USER_DESKTOP_DEP := $(USER_DESKTOP_C:%.c=build/%.c.d) $(USER_DESKTOP_S:%.S=build/%.S.d)

USER_DISPLAYD_C := $(shell find userspace/displayd -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_DISPLAYD_S := $(shell find userspace/displayd -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_DISPLAYD_OBJ := $(USER_DISPLAYD_C:%.c=build/%.c.o) $(USER_DISPLAYD_S:%.S=build/%.S.o)
USER_DISPLAYD_DEP := $(USER_DISPLAYD_C:%.c=build/%.c.d) $(USER_DISPLAYD_S:%.S=build/%.S.d)

USER_GUI_C := $(shell find userspace/gui -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_GUI_S := $(shell find userspace/gui -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_GUI_OBJ := $(USER_GUI_C:%.c=build/%.c.o) $(USER_GUI_S:%.S=build/%.S.o)
USER_GUI_DEP := $(USER_GUI_C:%.c=build/%.c.d) $(USER_GUI_S:%.S=build/%.S.d)

USER_SURFACEDEMO_C := $(shell find userspace/surfacedemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SURFACEDEMO_S := $(shell find userspace/surfacedemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SURFACEDEMO_OBJ := $(USER_SURFACEDEMO_C:%.c=build/%.c.o) $(USER_SURFACEDEMO_S:%.S=build/%.S.o)
USER_SURFACEDEMO_DEP := $(USER_SURFACEDEMO_C:%.c=build/%.c.d) $(USER_SURFACEDEMO_S:%.S=build/%.S.d)

USER_GUI2DEMO_C := $(shell find userspace/gui2demo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_GUI2DEMO_S := $(shell find userspace/gui2demo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_GUI2DEMO_OBJ := $(USER_GUI2DEMO_C:%.c=build/%.c.o) $(USER_GUI2DEMO_S:%.S=build/%.S.o)
USER_GUI2DEMO_DEP := $(USER_GUI2DEMO_C:%.c=build/%.c.d) $(USER_GUI2DEMO_S:%.S=build/%.S.d)

USER_GUIFAIL_C := $(shell find userspace/guifail -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_GUIFAIL_S := $(shell find userspace/guifail -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_GUIFAIL_OBJ := $(USER_GUIFAIL_C:%.c=build/%.c.o) $(USER_GUIFAIL_S:%.S=build/%.S.o)
USER_GUIFAIL_DEP := $(USER_GUIFAIL_C:%.c=build/%.c.d) $(USER_GUIFAIL_S:%.S=build/%.S.d)

USER_CALC_C := $(shell find userspace/calc -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_CALC_S := $(shell find userspace/calc -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_CALC_OBJ := $(USER_CALC_C:%.c=build/%.c.o) $(USER_CALC_S:%.S=build/%.S.o)
USER_CALC_DEP := $(USER_CALC_C:%.c=build/%.c.d) $(USER_CALC_S:%.S=build/%.S.d)

USER_NOTES_C := $(shell find userspace/notes -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_NOTES_S := $(shell find userspace/notes -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_NOTES_OBJ := $(USER_NOTES_C:%.c=build/%.c.o) $(USER_NOTES_S:%.S=build/%.S.o)
USER_NOTES_DEP := $(USER_NOTES_C:%.c=build/%.c.d) $(USER_NOTES_S:%.S=build/%.S.d)

USER_TEXTEDIT_C := $(shell find userspace/textedit -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TEXTEDIT_S := $(shell find userspace/textedit -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TEXTEDIT_OBJ := $(USER_TEXTEDIT_C:%.c=build/%.c.o) $(USER_TEXTEDIT_S:%.S=build/%.S.o)
USER_TEXTEDIT_DEP := $(USER_TEXTEDIT_C:%.c=build/%.c.d) $(USER_TEXTEDIT_S:%.S=build/%.S.d)

USER_PAINT_C := $(shell find userspace/paint -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_PAINT_S := $(shell find userspace/paint -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_PAINT_OBJ := $(USER_PAINT_C:%.c=build/%.c.o) $(USER_PAINT_S:%.S=build/%.S.o)
USER_PAINT_DEP := $(USER_PAINT_C:%.c=build/%.c.d) $(USER_PAINT_S:%.S=build/%.S.d)

USER_FILEMAN_C := $(shell find userspace/fileman -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_FILEMAN_S := $(shell find userspace/fileman -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_FILEMAN_OBJ := $(USER_FILEMAN_C:%.c=build/%.c.o) $(USER_FILEMAN_S:%.S=build/%.S.o)
USER_FILEMAN_DEP := $(USER_FILEMAN_C:%.c=build/%.c.d) $(USER_FILEMAN_S:%.S=build/%.S.d)

USER_POSIXDEMO_C := $(shell find userspace/posixdemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_POSIXDEMO_S := $(shell find userspace/posixdemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_POSIXDEMO_OBJ := $(USER_POSIXDEMO_C:%.c=build/%.c.o) $(USER_POSIXDEMO_S:%.S=build/%.S.o)
USER_POSIXDEMO_DEP := $(USER_POSIXDEMO_C:%.c=build/%.c.d) $(USER_POSIXDEMO_S:%.S=build/%.S.d)

USER_THREADSTRESS_C := $(shell find userspace/threadstress -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_THREADSTRESS_S := $(shell find userspace/threadstress -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_THREADSTRESS_OBJ := $(USER_THREADSTRESS_C:%.c=build/%.c.o) $(USER_THREADSTRESS_S:%.S=build/%.S.o)
USER_THREADSTRESS_DEP := $(USER_THREADSTRESS_C:%.c=build/%.c.d) $(USER_THREADSTRESS_S:%.S=build/%.S.d)

USER_EXECDEMO_C := $(shell find userspace/execdemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_EXECDEMO_S := $(shell find userspace/execdemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_EXECDEMO_OBJ := $(USER_EXECDEMO_C:%.c=build/%.c.o) $(USER_EXECDEMO_S:%.S=build/%.S.o)
USER_EXECDEMO_DEP := $(USER_EXECDEMO_C:%.c=build/%.c.d) $(USER_EXECDEMO_S:%.S=build/%.S.d)

USER_FDPROBE_C := $(shell find userspace/fdprobe -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_FDPROBE_S := $(shell find userspace/fdprobe -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_FDPROBE_OBJ := $(USER_FDPROBE_C:%.c=build/%.c.o) $(USER_FDPROBE_S:%.S=build/%.S.o)
USER_FDPROBE_DEP := $(USER_FDPROBE_C:%.c=build/%.c.d) $(USER_FDPROBE_S:%.S=build/%.S.d)

USER_LOCKPROBE_C := $(shell find userspace/lockprobe -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_LOCKPROBE_S := $(shell find userspace/lockprobe -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_LOCKPROBE_OBJ := $(USER_LOCKPROBE_C:%.c=build/%.c.o) $(USER_LOCKPROBE_S:%.S=build/%.S.o)
USER_LOCKPROBE_DEP := $(USER_LOCKPROBE_C:%.c=build/%.c.d) $(USER_LOCKPROBE_S:%.S=build/%.S.d)

USER_TTYDEMO_C := $(shell find userspace/ttydemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TTYDEMO_S := $(shell find userspace/ttydemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TTYDEMO_OBJ := $(USER_TTYDEMO_C:%.c=build/%.c.o) $(USER_TTYDEMO_S:%.S=build/%.S.o)
USER_TTYDEMO_DEP := $(USER_TTYDEMO_C:%.c=build/%.c.d) $(USER_TTYDEMO_S:%.S=build/%.S.d)

USER_JSONDEMO_C := $(shell find userspace/jsondemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_JSONDEMO_S := $(shell find userspace/jsondemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_JSONDEMO_OBJ := $(USER_JSONDEMO_C:%.c=build/%.c.o) $(USER_JSONDEMO_S:%.S=build/%.S.o)
USER_JSONDEMO_DEP := $(USER_JSONDEMO_C:%.c=build/%.c.d) $(USER_JSONDEMO_S:%.S=build/%.S.d)

USER_INIDEMO_C := $(shell find userspace/inidemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_INIDEMO_S := $(shell find userspace/inidemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_INIDEMO_OBJ := $(USER_INIDEMO_C:%.c=build/%.c.o) $(USER_INIDEMO_S:%.S=build/%.S.o)
USER_INIDEMO_DEP := $(USER_INIDEMO_C:%.c=build/%.c.d) $(USER_INIDEMO_S:%.S=build/%.S.d)

USER_LINEDEMO_C := $(shell find userspace/linedemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_LINEDEMO_S := $(shell find userspace/linedemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_LINEDEMO_OBJ := $(USER_LINEDEMO_C:%.c=build/%.c.o) $(USER_LINEDEMO_S:%.S=build/%.S.o)
USER_LINEDEMO_DEP := $(USER_LINEDEMO_C:%.c=build/%.c.d) $(USER_LINEDEMO_S:%.S=build/%.S.d)

USER_SQLITEDEMO_C := $(shell find userspace/sqlitedemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SQLITEDEMO_S := $(shell find userspace/sqlitedemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SQLITEDEMO_OBJ := $(USER_SQLITEDEMO_C:%.c=build/%.c.o) $(USER_SQLITEDEMO_S:%.S=build/%.S.o)
USER_SQLITEDEMO_DEP := $(USER_SQLITEDEMO_C:%.c=build/%.c.d) $(USER_SQLITEDEMO_S:%.S=build/%.S.d)

USER_ZLIBDEMO_C := $(shell find userspace/zlibdemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_ZLIBDEMO_S := $(shell find userspace/zlibdemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_ZLIBDEMO_OBJ := $(USER_ZLIBDEMO_C:%.c=build/%.c.o) $(USER_ZLIBDEMO_S:%.S=build/%.S.o)
USER_ZLIBDEMO_DEP := $(USER_ZLIBDEMO_C:%.c=build/%.c.d) $(USER_ZLIBDEMO_S:%.S=build/%.S.d)

USER_LUA_C := $(shell find userspace/lua -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_LUA_S := $(shell find userspace/lua -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_LUA_OBJ := $(USER_LUA_C:%.c=build/%.c.o) $(USER_LUA_S:%.S=build/%.S.o)
USER_LUA_DEP := $(USER_LUA_C:%.c=build/%.c.d) $(USER_LUA_S:%.S=build/%.S.d)

USER_UVDEMO_C := $(shell find userspace/uvdemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_UVDEMO_S := $(shell find userspace/uvdemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_UVDEMO_OBJ := $(USER_UVDEMO_C:%.c=build/%.c.o) $(USER_UVDEMO_S:%.S=build/%.S.o)
USER_UVDEMO_DEP := $(USER_UVDEMO_C:%.c=build/%.c.d) $(USER_UVDEMO_S:%.S=build/%.S.d)

USER_LIBUVDEMO_C := $(shell find userspace/libuvdemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_LIBUVDEMO_S := $(shell find userspace/libuvdemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_LIBUVDEMO_OBJ := $(USER_LIBUVDEMO_C:%.c=build/%.c.o) $(USER_LIBUVDEMO_S:%.S=build/%.S.o)
USER_LIBUVDEMO_DEP := $(USER_LIBUVDEMO_C:%.c=build/%.c.d) $(USER_LIBUVDEMO_S:%.S=build/%.S.d)

USER_NODEPROBE_C := $(shell find userspace/nodeprobe -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_NODEPROBE_S := $(shell find userspace/nodeprobe -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_NODEPROBE_OBJ := $(USER_NODEPROBE_C:%.c=build/%.c.o) $(USER_NODEPROBE_S:%.S=build/%.S.o)
USER_NODEPROBE_DEP := $(USER_NODEPROBE_C:%.c=build/%.c.d) $(USER_NODEPROBE_S:%.S=build/%.S.d)
USER_LIBCPROBE_C := $(shell find userspace/libcprobe -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_LIBCPROBE_S := $(shell find userspace/libcprobe -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_LIBCPROBE_OBJ := $(USER_LIBCPROBE_C:%.c=build/%.c.o) $(USER_LIBCPROBE_S:%.S=build/%.S.o)
USER_LIBCPROBE_DEP := $(USER_LIBCPROBE_C:%.c=build/%.c.d) $(USER_LIBCPROBE_S:%.S=build/%.S.d)

USER_TLSPROBE_C := $(shell find userspace/tlsprobe -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TLSPROBE_S := $(shell find userspace/tlsprobe -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TLSPROBE_OBJ := $(USER_TLSPROBE_C:%.c=build/%.c.o) $(USER_TLSPROBE_S:%.S=build/%.S.o)
USER_TLSPROBE_DEP := $(USER_TLSPROBE_C:%.c=build/%.c.d) $(USER_TLSPROBE_S:%.S=build/%.S.d)
USER_CXXPROBE_CC := $(shell find userspace/cxxprobe -type f -name '*.cc' 2>/dev/null | LC_ALL=C sort)
USER_CXXPROBE_OBJ := $(USER_CXXPROBE_CC:%.cc=build/%.cc.o)
USER_CXXPROBE_DEP := $(USER_CXXPROBE_CC:%.cc=build/%.cc.d)
USER_CXXSTLPROBE_CC := $(shell find userspace/cxxstlprobe -type f -name '*.cc' 2>/dev/null | LC_ALL=C sort)
USER_CXXSTLPROBE_OBJ := $(USER_CXXSTLPROBE_CC:%.cc=build/%.cc.o)
USER_CXXSTLPROBE_DEP := $(USER_CXXSTLPROBE_CC:%.cc=build/%.cc.d)

USER_LIB_C := $(shell find userspace/lib/src -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_LIB_CC := $(shell find userspace/lib/src -type f -name '*.cc' 2>/dev/null | LC_ALL=C sort)
USER_LIB_S := $(shell find userspace/lib/src -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_LIB_OBJ := $(USER_LIB_C:%.c=build/%.c.o) $(USER_LIB_CC:%.cc=build/%.cc.o) $(USER_LIB_S:%.S=build/%.S.o)
USER_LIB_DEP := $(USER_LIB_C:%.c=build/%.c.d) $(USER_LIB_CC:%.cc=build/%.cc.d) $(USER_LIB_S:%.S=build/%.S.d)

ZLIB_C := \
	ports/upstream/zlib/adler32.c \
	ports/upstream/zlib/compress.c \
	ports/upstream/zlib/crc32.c \
	ports/upstream/zlib/deflate.c \
	ports/upstream/zlib/infback.c \
	ports/upstream/zlib/inffast.c \
	ports/upstream/zlib/inflate.c \
	ports/upstream/zlib/inftrees.c \
	ports/upstream/zlib/trees.c \
	ports/upstream/zlib/uncompr.c \
	ports/upstream/zlib/zutil.c
ZLIB_OBJ := $(ZLIB_C:%.c=build/%.c.o)
ZLIB_DEP := $(ZLIB_C:%.c=build/%.c.d)

MINIZIP_C := \
	ports/upstream/zlib/contrib/minizip/ioapi.c \
	ports/upstream/zlib/contrib/minizip/zip.c \
	ports/upstream/zlib/contrib/minizip/minizip.c
MINIZIP_OBJ := $(MINIZIP_C:%.c=build/%.c.o)
MINIZIP_DEP := $(MINIZIP_C:%.c=build/%.c.d)

MINIUNZ_C := \
	ports/upstream/zlib/contrib/minizip/ioapi.c \
	ports/upstream/zlib/contrib/minizip/unzip.c
MINIUNZ_PATCHED_C := build/ports/srvros/minizip/miniunz.c
MINIUNZ_PATCHED_OBJ := build/ports/srvros/minizip/miniunz.c.o
MINIUNZ_OBJ := $(MINIUNZ_C:%.c=build/%.c.o) $(MINIUNZ_PATCHED_OBJ)
MINIUNZ_DEP := $(MINIUNZ_C:%.c=build/%.c.d) $(MINIUNZ_PATCHED_OBJ:.o=.d)

CJSON_C := ports/upstream/cjson/cJSON.c
CJSON_OBJ := $(CJSON_C:%.c=build/%.c.o)
CJSON_DEP := $(CJSON_C:%.c=build/%.c.d)

INI_C := ports/upstream/inih/ini.c
INI_OBJ := $(INI_C:%.c=build/%.c.o)
INI_DEP := $(INI_C:%.c=build/%.c.d)

LINENOISE_C := ports/srvros/linenoise.c
LINENOISE_OBJ := $(LINENOISE_C:%.c=build/%.c.o)
LINENOISE_DEP := $(LINENOISE_C:%.c=build/%.c.d)

UV_SRVROS_C := ports/srvros/uv.c
UV_SRVROS_OBJ := $(UV_SRVROS_C:%.c=build/%.c.o)
UV_SRVROS_DEP := $(UV_SRVROS_C:%.c=build/%.c.d)
UV_UPSTREAM_CORE_C := ports/upstream/libuv/src/version.c
UV_UPSTREAM_CORE_OBJ := $(UV_UPSTREAM_CORE_C:%.c=build/%.c.o)
UV_UPSTREAM_CORE_DEP := $(UV_UPSTREAM_CORE_C:%.c=build/%.c.d)

SQLITE_C := ports/upstream/sqlite/sqlite3.c
SQLITE_OBJ := $(SQLITE_C:%.c=build/%.c.o)
SQLITE_DEP := $(SQLITE_C:%.c=build/%.c.d)

BYACC_C := \
	ports/upstream/byacc/closure.c \
	ports/upstream/byacc/error.c \
	ports/upstream/byacc/graph.c \
	ports/upstream/byacc/lalr.c \
	ports/upstream/byacc/lr0.c \
	ports/upstream/byacc/main.c \
	ports/upstream/byacc/mkpar.c \
	ports/upstream/byacc/mstring.c \
	ports/upstream/byacc/output.c \
	ports/upstream/byacc/reader.c \
	ports/upstream/byacc/symtab.c \
	ports/upstream/byacc/verbose.c \
	ports/upstream/byacc/warshall.c \
	ports/upstream/byacc/yaccpar.c
BYACC_OBJ := $(BYACC_C:%.c=build/%.c.o)
BYACC_DEP := $(BYACC_C:%.c=build/%.c.d)

LUA_SRVROS_DIR := build/ports/lua-srvros
LUA_PREPARED := $(LUA_SRVROS_DIR)/.prepared
LUA_CORE_NAMES := \
	lapi.c \
	lauxlib.c \
	lbaselib.c \
	lcode.c \
	lcorolib.c \
	lctype.c \
	ldblib.c \
	ldebug.c \
	ldo.c \
	ldump.c \
	lfunc.c \
	lgc.c \
	liolib.c \
	llex.c \
	loadlib.c \
	lmathlib.c \
	lmem.c \
	lobject.c \
	loslib.c \
	lopcodes.c \
	lparser.c \
	lstate.c \
	lstring.c \
	lstrlib.c \
	ltable.c \
	ltablib.c \
	ltm.c \
	lundump.c \
	lutf8lib.c \
	lvm.c \
	lzio.c
LUA_CORE_C := $(addprefix $(LUA_SRVROS_DIR)/,$(LUA_CORE_NAMES))
LUA_CORE_OBJ := $(LUA_CORE_C:%.c=%.c.o)
LUA_CORE_DEP := $(LUA_CORE_C:%.c=%.c.d)

.PHONY: all
all: $(IMAGE)

.PHONY: check-host smoke
check-host:
	$(PYTHON) tools/check_host.py

# Sequential: each harness uses a disposable disk and its own QEMU instance.
smoke: $(IMAGE) $(USB_IMAGE) test-core
	$(PYTHON) -m unittest discover -s tools -p test_cli_harness.py
	$(PYTHON) tools/sysabi_smoke.py --qemu $(QEMU) --boot-wait $(SMOKE_BOOT_WAIT)
	$(PYTHON) tools/cli_smoke.py --qemu $(QEMU) --boot-wait $(SMOKE_BOOT_WAIT)
	$(PYTHON) tools/shell_edit_smoke.py --qemu $(QEMU) --boot-wait $(SMOKE_BOOT_WAIT) --send-delay 0.003
	$(PYTHON) tools/uefi_usb_smoke.py --qemu $(QEMU) --boot-wait $(SMOKE_BOOT_WAIT) --usb-type-text "echo usb-input-ok" --usb-type-expect "usb-input-ok"
	$(PYTHON) tools/displayd_smoke.py --qemu $(QEMU) --boot-wait $(SMOKE_BOOT_WAIT)

$(LIMINE_DIR)/.ready:
	mkdir -p build
	rm -rf $(LIMINE_DIR)
	git clone --depth=1 --branch=v11.x-binary https://github.com/limine-bootloader/limine.git $(LIMINE_DIR)
	git -C $(LIMINE_DIR) fetch --depth=1 origin $(LIMINE_COMMIT)
	git -C $(LIMINE_DIR) checkout --detach $(LIMINE_COMMIT)
ifeq ($(HOST_EXE),)
	$(MAKE) -C $(LIMINE_DIR) CC=$(HOST_CC)
endif
	touch $@

$(LIMINE_PROTOCOL_DIR)/.ready:
	mkdir -p build
	rm -rf $(LIMINE_PROTOCOL_DIR)
	git clone --depth=1 https://github.com/limine-bootloader/limine-protocol.git $(LIMINE_PROTOCOL_DIR)
	git -C $(LIMINE_PROTOCOL_DIR) fetch --depth=1 origin $(LIMINE_PROTOCOL_COMMIT)
	git -C $(LIMINE_PROTOCOL_DIR) checkout --detach $(LIMINE_PROTOCOL_COMMIT)
	touch $@

$(ZIG):
	$(PYTHON) tools/bootstrap_zig.py --version $(ZIG_VERSION) --output $(ZIG_DIR)

$(KERNEL): $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready $(KERNEL_OBJ) kernel/linker.ld
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(KERNEL_OBJ) -o $@

# Early-boot paths and trap-state helpers/diagnostics must not use compiler
# SSE: it may run before CR4.OSFXSR or clobber registers before they are saved.
build/kernel/src/arch/x86_64/fpu.c.o build/kernel/src/arch/x86_64/fpu_selftest.c.o build/kernel/src/console.c.o build/kernel/src/memory/mem.c.o build/kernel/src/latency.c.o: build/%.c.o: %.c $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready $(KERNEL_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -mno-sse -mno-sse2 -c $< -o $@

build/%.c.o: %.c $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready $(KERNEL_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.S.o: %.S $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready $(KERNEL_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(USER_INIT): $(ZIG) $(USER_CRT0_OBJ) $(USER_INIT_OBJ) $(USER_LIB_OBJ) userspace/linker.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_LDFLAGS) $(USER_CRT0_OBJ) $(USER_INIT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_LIB_A): $(ZIG) $(USER_LIB_OBJ)
	mkdir -p $(dir $@)
	rm -f $@
	$(AR) rcs $@ $(USER_LIB_OBJ)

.PHONY: srvros-sysroot
srvros-sysroot: $(USER_SYSROOT)/.ready

.PHONY: libc-audit
libc-audit: $(USER_LIB_A)
	$(PYTHON) tools/libc_symbol_audit.py --archive $(USER_LIB_A) --fail-missing

.PHONY: node-unresolved-audit
node-unresolved-audit:
	$(PYTHON) tools/node_unresolved_audit.py --show 0

$(USER_SYSROOT)/.ready: $(ZIG) $(USER_CRT0_OBJ) $(USER_LIB_A) userspace/app.ld $(USER_SYSROOT_HEADERS)
	rm -rf $(USER_SYSROOT)
	mkdir -p $(USER_SYSROOT)/include $(USER_SYSROOT)/lib
	cp -R shared/include/* $(USER_SYSROOT)/include/
	cp -R userspace/lib/include/* $(USER_SYSROOT)/include/
	cp userspace/app.ld $(USER_SYSROOT)/lib/app.ld
	cp $(USER_CRT0_OBJ) $(USER_SYSROOT)/lib/crt0.o
	cp $(USER_LIB_A) $(USER_SYSROOT)/lib/libsrvros.a
	printf '%s\n' 'x86_64-freestanding-none' > $(USER_SYSROOT)/TARGET
	touch $@

$(USER_HELLO): $(ZIG) $(USER_CRT0_OBJ) $(USER_HELLO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_HELLO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CAT): $(ZIG) $(USER_CRT0_OBJ) $(USER_CAT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CAT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_MORE): $(ZIG) $(USER_CRT0_OBJ) $(USER_MORE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_MORE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SH): $(ZIG) $(USER_CRT0_OBJ) $(USER_SH_OBJ) $(LINENOISE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SH_OBJ) $(LINENOISE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_LS): $(ZIG) $(USER_CRT0_OBJ) $(USER_LS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_LS_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_ECHO): $(ZIG) $(USER_CRT0_OBJ) $(USER_ECHO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_ECHO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_WRITE): $(ZIG) $(USER_CRT0_OBJ) $(USER_WRITE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_WRITE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_WC): $(ZIG) $(USER_CRT0_OBJ) $(USER_WC_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_WC_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CLEAR): $(ZIG) $(USER_CRT0_OBJ) $(USER_CLEAR_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CLEAR_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_PS): $(ZIG) $(USER_CRT0_OBJ) $(USER_PS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_PS_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_KILL): $(ZIG) $(USER_CRT0_OBJ) $(USER_KILL_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_KILL_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_WHICH): $(ZIG) $(USER_CRT0_OBJ) $(USER_WHICH_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_WHICH_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_ENV): $(ZIG) $(USER_CRT0_OBJ) $(USER_ENV_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_ENV_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_PWD): $(ZIG) $(USER_CRT0_OBJ) $(USER_PWD_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_PWD_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TRUE): $(ZIG) $(USER_CRT0_OBJ) $(USER_TRUE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TRUE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_FALSE): $(ZIG) $(USER_CRT0_OBJ) $(USER_FALSE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_FALSE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SLEEP): $(ZIG) $(USER_CRT0_OBJ) $(USER_SLEEP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SLEEP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_DATE): $(ZIG) $(USER_CRT0_OBJ) $(USER_DATE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_DATE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TOUCH): $(ZIG) $(USER_CRT0_OBJ) $(USER_TOUCH_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TOUCH_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_MKTEMP): $(ZIG) $(USER_CRT0_OBJ) $(USER_MKTEMP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_MKTEMP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_BASENAME): $(ZIG) $(USER_CRT0_OBJ) $(USER_BASENAME_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_BASENAME_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_DIRNAME): $(ZIG) $(USER_CRT0_OBJ) $(USER_DIRNAME_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_DIRNAME_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_GREP): $(ZIG) $(USER_CRT0_OBJ) $(USER_GREP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_GREP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_HEAD): $(ZIG) $(USER_CRT0_OBJ) $(USER_HEAD_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_HEAD_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TAIL): $(ZIG) $(USER_CRT0_OBJ) $(USER_TAIL_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TAIL_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TEE): $(ZIG) $(USER_CRT0_OBJ) $(USER_TEE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TEE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_UNAME): $(ZIG) $(USER_CRT0_OBJ) $(USER_UNAME_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_UNAME_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_HOSTNAME): $(ZIG) $(USER_CRT0_OBJ) $(USER_HOSTNAME_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_HOSTNAME_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_UPTIME): $(ZIG) $(USER_CRT0_OBJ) $(USER_UPTIME_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_UPTIME_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_FIND): $(ZIG) $(USER_CRT0_OBJ) $(USER_FIND_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_FIND_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_DU): $(ZIG) $(USER_CRT0_OBJ) $(USER_DU_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_DU_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_DF): $(ZIG) $(USER_CRT0_OBJ) $(USER_DF_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_DF_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SORT): $(ZIG) $(USER_CRT0_OBJ) $(USER_SORT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SORT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_UNIQ): $(ZIG) $(USER_CRT0_OBJ) $(USER_UNIQ_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_UNIQ_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CUT): $(ZIG) $(USER_CRT0_OBJ) $(USER_CUT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CUT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_XARGS): $(ZIG) $(USER_CRT0_OBJ) $(USER_XARGS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_XARGS_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SEQ): $(ZIG) $(USER_CRT0_OBJ) $(USER_SEQ_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SEQ_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_REALPATH): $(ZIG) $(USER_CRT0_OBJ) $(USER_REALPATH_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_REALPATH_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_ID): $(ZIG) $(USER_CRT0_OBJ) $(USER_ID_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_ID_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_WHOAMI): $(ZIG) $(USER_CRT0_OBJ) $(USER_WHOAMI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_WHOAMI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_READLINK): $(ZIG) $(USER_CRT0_OBJ) $(USER_READLINK_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_READLINK_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CMP): $(ZIG) $(USER_CRT0_OBJ) $(USER_CMP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CMP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_YES): $(ZIG) $(USER_CRT0_OBJ) $(USER_YES_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_YES_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_INSTALL): $(ZIG) $(USER_CRT0_OBJ) $(USER_INSTALL_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_INSTALL_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_DIFF): $(ZIG) $(USER_CRT0_OBJ) $(USER_DIFF_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_DIFF_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TAR): $(ZIG) $(USER_CRT0_OBJ) $(USER_TAR_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TAR_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_GZIP): $(ZIG) $(USER_CRT0_OBJ) $(USER_GZIP_OBJ) $(ZLIB_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_GZIP_OBJ) $(ZLIB_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_PATCH): $(ZIG) $(USER_CRT0_OBJ) $(USER_PATCH_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_PATCH_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_MAKE): $(ZIG) $(USER_CRT0_OBJ) $(USER_MAKE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_MAKE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_BYACC): $(ZIG) $(USER_CRT0_OBJ) $(BYACC_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(BYACC_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SED): $(ZIG) $(USER_CRT0_OBJ) $(USER_SED_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SED_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_EXPR): $(ZIG) $(USER_CRT0_OBJ) $(USER_EXPR_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_EXPR_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_PRINTF): $(ZIG) $(USER_CRT0_OBJ) $(USER_PRINTF_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_PRINTF_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TR): $(ZIG) $(USER_CRT0_OBJ) $(USER_TR_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TR_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_DD): $(ZIG) $(USER_CRT0_OBJ) $(USER_DD_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_DD_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_ED): $(ZIG) $(USER_CRT0_OBJ) $(USER_ED_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_ED_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_POSIXUTILS): $(ZIG) $(USER_CRT0_OBJ) $(USER_POSIXUTILS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_POSIXUTILS_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_STAT): $(ZIG) $(USER_CRT0_OBJ) $(USER_STAT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_STAT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CHMOD): $(ZIG) $(USER_CRT0_OBJ) $(USER_CHMOD_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CHMOD_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CP): $(ZIG) $(USER_CRT0_OBJ) $(USER_CP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_RM): $(ZIG) $(USER_CRT0_OBJ) $(USER_RM_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_RM_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_MKDIR): $(ZIG) $(USER_CRT0_OBJ) $(USER_MKDIR_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_MKDIR_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_MV): $(ZIG) $(USER_CRT0_OBJ) $(USER_MV_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_MV_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TAP): $(ZIG) $(USER_CRT0_OBJ) $(USER_TAP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TAP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SVSCAN): $(ZIG) $(USER_CRT0_OBJ) $(USER_SVSCAN_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SVSCAN_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_WEBD): $(ZIG) $(USER_CRT0_OBJ) $(USER_WEBD_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_WEBD_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_HTTPGET): $(ZIG) $(USER_CRT0_OBJ) $(USER_HTTPGET_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_HTTPGET_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_UDPDNS): $(ZIG) $(USER_CRT0_OBJ) $(USER_UDPDNS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_UDPDNS_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_UDPECHO): $(ZIG) $(USER_CRT0_OBJ) $(USER_UDPECHO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_UDPECHO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_NETSTAT): $(ZIG) $(USER_CRT0_OBJ) $(USER_NETSTAT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_NETSTAT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_IFCONFIG): $(ZIG) $(USER_CRT0_OBJ) $(USER_IFCONFIG_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_IFCONFIG_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_ROUTE): $(ZIG) $(USER_CRT0_OBJ) $(USER_ROUTE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_ROUTE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_ARP): $(ZIG) $(USER_CRT0_OBJ) $(USER_ARP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_ARP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_PING): $(ZIG) $(USER_CRT0_OBJ) $(USER_PING_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_PING_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_HOST): $(ZIG) $(USER_CRT0_OBJ) $(USER_HOST_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_HOST_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_NETCHECK): $(ZIG) $(USER_CRT0_OBJ) $(USER_NETCHECK_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_NETCHECK_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_NETABI): $(ZIG) $(USER_CRT0_OBJ) $(USER_NETABI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_NETABI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TCPSTRESS): $(ZIG) $(USER_CRT0_OBJ) $(USER_TCPSTRESS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TCPSTRESS_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TCPPROBE): $(ZIG) $(USER_CRT0_OBJ) $(USER_TCPPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TCPPROBE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SYSABI): $(ZIG) $(USER_CRT0_OBJ) $(USER_SYSABI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SYSABI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SPIN): $(ZIG) $(USER_CRT0_OBJ) $(USER_SPIN_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SPIN_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_FPDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_FPDEMO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_FPDEMO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_UI): $(ZIG) $(USER_CRT0_OBJ) $(USER_UI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_UI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_DESKTOP): $(ZIG) $(USER_CRT0_OBJ) $(USER_DESKTOP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_DESKTOP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_DISPLAYD): $(ZIG) $(USER_CRT0_OBJ) $(USER_DISPLAYD_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_DISPLAYD_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_GUI): $(ZIG) $(USER_CRT0_OBJ) $(USER_GUI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_GUI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SURFACEDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_SURFACEDEMO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SURFACEDEMO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_GUI2DEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_GUI2DEMO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_GUI2DEMO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_GUIFAIL): $(ZIG) $(USER_CRT0_OBJ) $(USER_GUIFAIL_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_GUIFAIL_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CALC): $(ZIG) $(USER_CRT0_OBJ) $(USER_CALC_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CALC_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_NOTES): $(ZIG) $(USER_CRT0_OBJ) $(USER_NOTES_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_NOTES_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TEXTEDIT): $(ZIG) $(USER_CRT0_OBJ) $(USER_TEXTEDIT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TEXTEDIT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_PAINT): $(ZIG) $(USER_CRT0_OBJ) $(USER_PAINT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_PAINT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_FILEMAN): $(ZIG) $(USER_CRT0_OBJ) $(USER_FILEMAN_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_FILEMAN_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_POSIXDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_POSIXDEMO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_POSIXDEMO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_THREADSTRESS): $(ZIG) $(USER_CRT0_OBJ) $(USER_THREADSTRESS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_THREADSTRESS_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_EXECDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_EXECDEMO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_EXECDEMO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_FDPROBE): $(ZIG) $(USER_CRT0_OBJ) $(USER_FDPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_FDPROBE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_LOCKPROBE): $(ZIG) $(USER_CRT0_OBJ) $(USER_LOCKPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_LOCKPROBE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TTYDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_TTYDEMO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TTYDEMO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_JSONDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_JSONDEMO_OBJ) $(CJSON_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_JSONDEMO_OBJ) $(CJSON_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_INIDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_INIDEMO_OBJ) $(INI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_INIDEMO_OBJ) $(INI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_LINEDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_LINEDEMO_OBJ) $(LINENOISE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_LINEDEMO_OBJ) $(LINENOISE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SQLITEDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_SQLITEDEMO_OBJ) $(SQLITE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_SQLITEDEMO_OBJ) $(SQLITE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_ZLIBDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_ZLIBDEMO_OBJ) $(ZLIB_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_ZLIBDEMO_OBJ) $(ZLIB_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_MINIZIP): $(ZIG) $(USER_CRT0_OBJ) $(MINIZIP_OBJ) $(ZLIB_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(MINIZIP_OBJ) $(ZLIB_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_MINIUNZ): $(ZIG) $(USER_CRT0_OBJ) $(MINIUNZ_OBJ) $(ZLIB_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(MINIUNZ_OBJ) $(ZLIB_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_LUA): $(ZIG) $(USER_CRT0_OBJ) $(USER_LUA_OBJ) $(LUA_CORE_OBJ) $(LINENOISE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_LUA_OBJ) $(LUA_CORE_OBJ) $(LINENOISE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_UVDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_UVDEMO_OBJ) $(UV_SRVROS_OBJ) $(UV_UPSTREAM_CORE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_UVDEMO_OBJ) $(UV_SRVROS_OBJ) $(UV_UPSTREAM_CORE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_LIBUVDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_LIBUVDEMO_OBJ) $(UV_SRVROS_OBJ) $(UV_UPSTREAM_CORE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_LIBUVDEMO_OBJ) $(UV_SRVROS_OBJ) $(UV_UPSTREAM_CORE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_NODEPROBE): $(ZIG) $(USER_CRT0_OBJ) $(USER_NODEPROBE_OBJ) $(UV_SRVROS_OBJ) $(UV_UPSTREAM_CORE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_NODEPROBE_OBJ) $(UV_SRVROS_OBJ) $(UV_UPSTREAM_CORE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_LIBCPROBE): $(ZIG) $(USER_CRT0_OBJ) $(USER_LIBCPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_LIBCPROBE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TLSPROBE): $(ZIG) $(USER_CRT0_OBJ) $(USER_TLSPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TLSPROBE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CXXPROBE): $(ZIG) $(USER_CRT0_OBJ) $(USER_CXXPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CXXPROBE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CXXSTLPROBE): $(ZIG) $(USER_CRT0_OBJ) $(USER_CXXSTLPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CXXSTLPROBE_OBJ) $(USER_LIB_OBJ) -o $@

build/userspace/%.c.o: userspace/%.c $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/userspace/cxxstlprobe/%.cc.o: userspace/cxxstlprobe/%.cc $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CXX) $(USER_STL_CXXFLAGS) -c $< -o $@

build/userspace/%.cc.o: userspace/%.cc $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CXX) $(USER_CXXFLAGS) -c $< -o $@

build/userspace/lib/src/%.cc.o: userspace/lib/src/%.cc $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CXX) $(USER_CXXFLAGS) -c $< -o $@

build/userspace/lua/%.c.o: userspace/lua/%.c $(ZIG) $(LUA_PREPARED) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I $(LUA_SRVROS_DIR) -I ports/upstream/linenoise -c $< -o $@

build/userspace/zlibdemo/%.c.o: userspace/zlibdemo/%.c $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/zlib -c $< -o $@

build/userspace/gzip/%.c.o: userspace/gzip/%.c $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/zlib -c $< -o $@

build/userspace/jsondemo/%.c.o: userspace/jsondemo/%.c $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/cjson -c $< -o $@

build/userspace/inidemo/%.c.o: userspace/inidemo/%.c $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/inih -c $< -o $@

build/userspace/linedemo/%.c.o: userspace/linedemo/%.c $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/linenoise -c $< -o $@

build/userspace/sqlitedemo/%.c.o: userspace/sqlitedemo/%.c $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/sqlite -c $< -o $@

build/userspace/uvdemo/%.c.o: userspace/uvdemo/%.c $(ZIG) ports/srvros/uv.h $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/srvros -c $< -o $@

build/userspace/libuvdemo/%.c.o: userspace/libuvdemo/%.c $(ZIG) ports/srvros/uv.h $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/srvros -c $< -o $@

build/userspace/nodeprobe/%.c.o: userspace/nodeprobe/%.c $(ZIG) ports/srvros/uv.h $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/srvros -c $< -o $@

build/userspace/sh/%.c.o: userspace/sh/%.c $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/linenoise -c $< -o $@

build/userspace/%.S.o: userspace/%.S $(ZIG) $(USER_SYSROOT_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/ports/upstream/zlib/%.c.o: ports/upstream/zlib/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/zlib -Wno-error -c $< -o $@

build/ports/upstream/zlib/contrib/minizip/%.c.o: ports/upstream/zlib/contrib/minizip/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/zlib -I ports/upstream/zlib/contrib/minizip -DMINIZIP_FOPEN_NO_64 -DZ_BUFSIZE=16384 -Wno-error -Wno-unused-parameter -Wno-sign-compare -c $< -o $@

$(MINIUNZ_PATCHED_C): ports/upstream/zlib/contrib/minizip/miniunz.c
	mkdir -p $(dir $@)
	$(PYTHON) -c "from pathlib import Path; src=Path('$<'); dst=Path('$@'); text=src.read_text(); text=text.replace('char filename_inzip[65536+1];', 'char filename_inzip[MAXFILENAME+1];'); dst.write_text(text)"

$(MINIUNZ_PATCHED_OBJ): $(MINIUNZ_PATCHED_C) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/zlib -I ports/upstream/zlib/contrib/minizip -DMINIZIP_FOPEN_NO_64 -DZ_BUFSIZE=16384 -Wno-error -Wno-unused-parameter -Wno-sign-compare -c $< -o $@

build/ports/upstream/cjson/%.c.o: ports/upstream/cjson/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/cjson -Wno-error -c $< -o $@

build/ports/upstream/inih/%.c.o: ports/upstream/inih/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/inih -Wno-error -c $< -o $@

build/ports/srvros/%.c.o: ports/srvros/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/linenoise -c $< -o $@

build/ports/upstream/libuv/src/version.c.o: ports/upstream/libuv/src/version.c $(ZIG) ports/srvros/uv.h
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/srvros -I ports/upstream/libuv/include -c $< -o $@

build/ports/upstream/sqlite/%.c.o: ports/upstream/sqlite/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/sqlite -DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_OMIT_LOCALTIME -Wno-error -Wno-unused-parameter -Wno-sign-compare -c $< -o $@

build/ports/upstream/byacc/%.c.o: ports/upstream/byacc/%.c $(ZIG) ports/srvros/byacc_config.h
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/byacc -include ports/srvros/byacc_config.h -Wno-error -c $< -o $@

$(LUA_PREPARED): tools/prepare_lua_port.py $(shell find ports/upstream/lua -maxdepth 1 -type f 2>/dev/null | LC_ALL=C sort)
	mkdir -p $(LUA_SRVROS_DIR)
	$(PYTHON) tools/prepare_lua_port.py ports/upstream/lua $(LUA_SRVROS_DIR)

# The stamp is written after the generated sources. It must not make each
# source look perpetually out of date. Objects still depend on it normally.
$(LUA_CORE_C): | $(LUA_PREPARED)
	@test -f $@ || { echo "Missing $@; remove $(LUA_PREPARED) and rerun make to regenerate Lua." >&2; exit 1; }

$(LUA_SRVROS_DIR)/%.c.o: $(LUA_SRVROS_DIR)/%.c $(ZIG) $(LUA_PREPARED) ports/srvros/lua_config.h
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I $(LUA_SRVROS_DIR) -include ports/srvros/lua_config.h -DNDEBUG -Dl_signalT=int -Wno-error -Wno-unused-parameter -Wno-unused-function -Wno-missing-braces -c $< -o $@

$(EXFAT_IMAGE): $(USER_LIFETIMEPROBE) userspace/examples/tap-diagnostics.sh userspace/examples/tap-checks.sh userspace/examples/trackpad-checks.sh userspace/examples/device-checks.sh userspace/examples/network-trial.sh tools/audio_fixture.py $(USER_DESKFIND) $(USER_FINDFSWORKER) $(USER_DESKNET) $(USER_DESKSKETCH) $(USER_SHOTWORKER) $(USER_DESKCLOCK) $(USER_DESKHELP) $(USER_DESKVIEW) $(USER_IMAGEWORKER) tools/pack_runtime_elf.py $(USER_CLOCKPROBE) $(USER_DESKTERM) $(USER_PTYPROBE) $(USER_DESKTOOLS) $(USER_DESKFS) $(USER_DESKIO) $(USER_DESKD) $(USER_DESKCLIENT) $(USER_DESKSTUDY) tools/mk_exfat_image.py $(USER_HELLO) $(USER_CAT) $(USER_MORE) $(USER_SH) $(USER_LS) $(USER_ECHO) $(USER_WRITE) $(USER_WC) $(USER_CLEAR) $(USER_PS) $(USER_KILL) $(USER_WHICH) $(USER_ENV) $(USER_PWD) $(USER_TRUE) $(USER_FALSE) $(USER_SLEEP) $(USER_DATE) $(USER_TOUCH) $(USER_MKTEMP) $(USER_BASENAME) $(USER_DIRNAME) $(USER_GREP) $(USER_HEAD) $(USER_TAIL) $(USER_TEE) $(USER_UNAME) $(USER_HOSTNAME) $(USER_UPTIME) $(USER_FIND) $(USER_DU) $(USER_DF) $(USER_SORT) $(USER_UNIQ) $(USER_CUT) $(USER_XARGS) $(USER_SEQ) $(USER_REALPATH) $(USER_ID) $(USER_WHOAMI) $(USER_READLINK) $(USER_CMP) $(USER_YES) $(USER_INSTALL) $(USER_DIFF) $(USER_TAR) $(USER_GZIP) $(USER_MINIZIP) $(USER_MINIUNZ) $(USER_PATCH) $(USER_MAKE) $(USER_BYACC) $(USER_SED) $(USER_EXPR) $(USER_PRINTF) $(USER_TR) $(USER_DD) $(USER_ED) $(USER_POSIXUTILS) $(USER_STAT) $(USER_CHMOD) $(USER_CP) $(USER_RM) $(USER_MKDIR) $(USER_MV) $(USER_TAP) $(USER_SVSCAN) $(USER_WEBD) $(USER_HTTPGET) $(USER_UDPDNS) $(USER_UDPECHO) $(USER_NETSTAT) $(USER_IFCONFIG) $(USER_ROUTE) $(USER_ARP) $(USER_PING) $(USER_HOST) $(USER_NETCHECK) $(USER_NETABI) $(USER_TCPSTRESS) $(USER_TCPPROBE) $(USER_SYSABI) $(USER_SPIN) $(USER_FPDEMO) $(USER_UI) $(USER_DESKTOP) $(USER_DISPLAYD) $(USER_GUI) $(USER_SURFACEDEMO) $(USER_GUI2DEMO) $(USER_GUIFAIL) $(USER_CALC) $(USER_NOTES) $(USER_TEXTEDIT) $(USER_PAINT) $(USER_FILEMAN) $(USER_POSIXDEMO) $(USER_THREADSTRESS) $(USER_EXECDEMO) $(USER_FDPROBE) $(USER_LOCKPROBE) $(USER_TTYDEMO) $(USER_JSONDEMO) $(USER_INIDEMO) $(USER_LINEDEMO) $(USER_SQLITEDEMO) $(USER_ZLIBDEMO) $(USER_LUA) $(USER_UVDEMO) $(USER_LIBUVDEMO) $(USER_NODEPROBE) $(USER_LIBCPROBE) $(USER_TLSPROBE) $(USER_CXXPROBE) $(USER_CXXSTLPROBE) $(USER_PLAYWAV)
	mkdir -p $(dir $@)
	$(PYTHON) tools/mk_exfat_image.py $@ hello=$(USER_HELLO) cat=$(USER_CAT) more=$(USER_MORE) sh=$(USER_SH) ls=$(USER_LS) echo=$(USER_ECHO) write=$(USER_WRITE) wc=$(USER_WC) clear=$(USER_CLEAR) ps=$(USER_PS) kill=$(USER_KILL) which=$(USER_WHICH) env=$(USER_ENV) pwd=$(USER_PWD) true=$(USER_TRUE) false=$(USER_FALSE) sleep=$(USER_SLEEP) date=$(USER_DATE) touch=$(USER_TOUCH) mktemp=$(USER_MKTEMP) basename=$(USER_BASENAME) dirname=$(USER_DIRNAME) grep=$(USER_GREP) head=$(USER_HEAD) tail=$(USER_TAIL) tee=$(USER_TEE) uname=$(USER_UNAME) hostname=$(USER_HOSTNAME) uptime=$(USER_UPTIME) find=$(USER_FIND) du=$(USER_DU) df=$(USER_DF) sort=$(USER_SORT) uniq=$(USER_UNIQ) cut=$(USER_CUT) xargs=$(USER_XARGS) seq=$(USER_SEQ) realpath=$(USER_REALPATH) id=$(USER_ID) whoami=$(USER_WHOAMI) readlink=$(USER_READLINK) cmp=$(USER_CMP) yes=$(USER_YES) install=$(USER_INSTALL) diff=$(USER_DIFF) tar=$(USER_TAR) gzip=$(USER_GZIP) gunzip=$(USER_GZIP) minizip=$(USER_MINIZIP) miniunz=$(USER_MINIUNZ) patch=$(USER_PATCH) make=$(USER_MAKE) byacc=$(USER_BYACC) sed=$(USER_SED) expr=$(USER_EXPR) printf=$(USER_PRINTF) tr=$(USER_TR) dd=$(USER_DD) ed=$(USER_ED) ln=$(USER_POSIXUTILS) sync=$(USER_POSIXUTILS) test=$(USER_POSIXUTILS) '[=$(USER_POSIXUTILS)' cksum=$(USER_POSIXUTILS) sum=$(USER_POSIXUTILS) comm=$(USER_POSIXUTILS) paste=$(USER_POSIXUTILS) join=$(USER_POSIXUTILS) split=$(USER_POSIXUTILS) od=$(USER_POSIXUTILS) hexdump=$(USER_POSIXUTILS) strings=$(USER_POSIXUTILS) file=$(USER_POSIXUTILS) tty=$(USER_POSIXUTILS) stty=$(USER_POSIXUTILS) time=$(USER_POSIXUTILS) timeout=$(USER_POSIXUTILS) nohup=$(USER_POSIXUTILS) nice=$(USER_POSIXUTILS) stat=$(USER_STAT) chmod=$(USER_CHMOD) cp=$(USER_CP) rm=$(USER_RM) mkdir=$(USER_MKDIR) mv=$(USER_MV) tap=$(USER_TAP) svscan=$(USER_SVSCAN) webd=$(USER_WEBD) httpget=$(USER_HTTPGET) udpdns=$(USER_UDPDNS) udpecho=$(USER_UDPECHO) netstat=$(USER_NETSTAT) ifconfig=$(USER_IFCONFIG) route=$(USER_ROUTE) arp=$(USER_ARP) ping=$(USER_PING) host=$(USER_HOST) netcheck=$(USER_NETCHECK) netabi=$(USER_NETABI) tcpstress=$(USER_TCPSTRESS) tcpprobe=$(USER_TCPPROBE) sysabi=$(USER_SYSABI) spin=$(USER_SPIN) fpdemo=$(USER_FPDEMO) ui=$(USER_UI) desktop=$(USER_DESKTOP) displayd=$(USER_DISPLAYD) deskstudy=$(USER_DESKSTUDY) deskd=$(USER_DESKD) deskclient=$(USER_DESKCLIENT) deskio=$(USER_DESKIO) desktools=$(USER_DESKTOOLS) deskfs=$(USER_DESKFS) shotworker=$(USER_SHOTWORKER) desksketch=$(USER_DESKSKETCH) imageworker=$(USER_IMAGEWORKER) deskview=$(USER_DESKVIEW) deskhelp=$(USER_DESKHELP) deskfind=$(USER_DESKFIND) deskfind-worker=$(USER_FINDFSWORKER) desknet=$(USER_DESKNET) deskclock=$(USER_DESKCLOCK) clockprobe=$(USER_CLOCKPROBE) lifetimeprobe=$(USER_LIFETIMEPROBE) ptyprobe=$(USER_PTYPROBE) deskterm=$(USER_DESKTERM) gui=$(USER_GUI) surfacedemo=$(USER_SURFACEDEMO) gui2demo=$(USER_GUI2DEMO) guifail=$(USER_GUIFAIL) calc=$(USER_CALC) notes=$(USER_NOTES) textedit=$(USER_TEXTEDIT) paint=$(USER_PAINT) fileman=$(USER_FILEMAN) posixdemo=$(USER_POSIXDEMO) threadstress=$(USER_THREADSTRESS) execdemo=$(USER_EXECDEMO) fdprobe=$(USER_FDPROBE) lockprobe=$(USER_LOCKPROBE) ttydemo=$(USER_TTYDEMO) jsondemo=$(USER_JSONDEMO) inidemo=$(USER_INIDEMO) linedemo=$(USER_LINEDEMO) sqlitedemo=$(USER_SQLITEDEMO) zlibdemo=$(USER_ZLIBDEMO) lua=$(USER_LUA) uvdemo=$(USER_UVDEMO) libuvdemo=$(USER_LIBUVDEMO) nodeprobe=$(USER_NODEPROBE) libcprobe=$(USER_LIBCPROBE) tlsprobe=$(USER_TLSPROBE) cxxprobe=$(USER_CXXPROBE) cxxstlprobe=$(USER_CXXSTLPROBE) playwav=$(USER_PLAYWAV)

$(SECOND_EXFAT_IMAGE): $(EXFAT_IMAGE)
	cp $(EXFAT_IMAGE) $(SECOND_EXFAT_IMAGE)

$(NODE_RUNTIME_ELF): ports/srvros/node/probe-srvros-link.ps1 $(USER_CRT0_OBJ) $(USER_LIB_A) userspace/app.ld
	powershell -ExecutionPolicy Bypass -File ports/srvros/node/probe-srvros-link.ps1

$(NODE_RUNTIME_STRIPPED): $(NODE_RUNTIME_ELF)
	mkdir -p $(dir $@)
	llvm-strip -o $@ $(NODE_RUNTIME_ELF)

$(NODE_RUNTIME_EXFAT): tools/mk_exfat_image.py $(NODE_RUNTIME_STRIPPED)
	mkdir -p $(dir $@)
	$(PYTHON) tools/mk_exfat_image.py $@ node=$(NODE_RUNTIME_STRIPPED)

.PHONY: node-runtime-image
node-runtime-image: $(NODE_RUNTIME_EXFAT)

$(INITRAMFS): tools/pack_runtime_elf.py $(USER_INIT) $(USER_SH) $(USER_LS) $(USER_ECHO) $(USER_WRITE) $(USER_WC) $(USER_CLEAR) $(USER_PS) $(USER_KILL) $(USER_WHICH) $(USER_ENV) $(USER_PWD) $(USER_TRUE) $(USER_FALSE) $(USER_SLEEP) $(USER_DATE) $(USER_TOUCH) $(USER_MKTEMP) $(USER_BASENAME) $(USER_DIRNAME) $(USER_GREP) $(USER_HEAD) $(USER_TAIL) $(USER_TEE) $(USER_MORE) $(USER_UNAME) $(USER_HOSTNAME) $(USER_UPTIME) $(USER_FIND) $(USER_DU) $(USER_DF) $(USER_SORT) $(USER_UNIQ) $(USER_CUT) $(USER_XARGS) $(USER_SEQ) $(USER_REALPATH) $(USER_ID) $(USER_WHOAMI) $(USER_READLINK) $(USER_CMP) $(USER_YES) $(USER_INSTALL) $(USER_DIFF) $(USER_TAR) $(USER_GZIP) $(USER_MINIZIP) $(USER_MINIUNZ) $(USER_PATCH) $(USER_MAKE) $(USER_BYACC) $(USER_SED) $(USER_EXPR) $(USER_PRINTF) $(USER_TR) $(USER_DD) $(USER_ED) $(USER_POSIXUTILS) $(USER_STAT) $(USER_CHMOD) $(USER_CP) $(USER_RM) $(USER_MKDIR) $(USER_MV) $(USER_TAP) $(USER_SVSCAN) $(USER_WEBD) $(USER_HTTPGET) $(USER_UDPDNS) $(USER_UDPECHO) $(USER_NETSTAT) $(USER_IFCONFIG) $(USER_ROUTE) $(USER_ARP) $(USER_PING) $(USER_HOST) $(USER_NETCHECK) $(USER_NETABI) $(USER_TCPSTRESS) $(USER_TCPPROBE) $(USER_SYSABI) $(USER_SPIN) $(USER_FPDEMO) $(USER_UI) $(USER_DESKTOP) $(USER_DISPLAYD) $(USER_GUI) $(USER_SURFACEDEMO) $(USER_GUI2DEMO) $(USER_GUIFAIL) $(USER_CALC) $(USER_NOTES) $(USER_TEXTEDIT) $(USER_PAINT) $(USER_FILEMAN) $(USER_POSIXDEMO) $(USER_THREADSTRESS) $(USER_EXECDEMO) $(USER_FDPROBE) $(USER_LOCKPROBE) $(USER_TTYDEMO) $(USER_JSONDEMO) $(USER_INIDEMO) $(USER_LINEDEMO) $(USER_SQLITEDEMO) $(USER_ZLIBDEMO) $(USER_LUA) $(USER_UVDEMO) $(USER_LIBUVDEMO) $(USER_NODEPROBE) $(USER_LIBCPROBE) $(USER_TLSPROBE) $(USER_CXXPROBE) $(USER_CXXSTLPROBE) $(EXFAT_IMAGE) $(shell find initramfs -type f 2>/dev/null | LC_ALL=C sort) $(USER_PLAYWAV)
	mkdir -p build
	rm -rf $(INITRAMFS_ROOT)
	mkdir -p $(INITRAMFS_ROOT)
	cp -R initramfs/. $(INITRAMFS_ROOT)/
	cp $(USER_INIT) $(INITRAMFS_ROOT)/init
	cp $(USER_SH) $(INITRAMFS_ROOT)/sh
	cp $(USER_LS) $(INITRAMFS_ROOT)/ls
	cp $(USER_ECHO) $(INITRAMFS_ROOT)/echo
	cp $(USER_WRITE) $(INITRAMFS_ROOT)/write
	cp $(USER_WC) $(INITRAMFS_ROOT)/wc
	cp $(USER_CLEAR) $(INITRAMFS_ROOT)/clear
	cp $(USER_PS) $(INITRAMFS_ROOT)/ps
	cp $(USER_KILL) $(INITRAMFS_ROOT)/kill
	cp $(USER_WHICH) $(INITRAMFS_ROOT)/which
	cp $(USER_ENV) $(INITRAMFS_ROOT)/env
	cp $(USER_PWD) $(INITRAMFS_ROOT)/pwd
	cp $(USER_TRUE) $(INITRAMFS_ROOT)/true
	cp $(USER_FALSE) $(INITRAMFS_ROOT)/false
	cp $(USER_SLEEP) $(INITRAMFS_ROOT)/sleep
	cp $(USER_DATE) $(INITRAMFS_ROOT)/date
	cp $(USER_TOUCH) $(INITRAMFS_ROOT)/touch
	cp $(USER_MKTEMP) $(INITRAMFS_ROOT)/mktemp
	cp $(USER_BASENAME) $(INITRAMFS_ROOT)/basename
	cp $(USER_DIRNAME) $(INITRAMFS_ROOT)/dirname
	cp $(USER_GREP) $(INITRAMFS_ROOT)/grep
	cp $(USER_HEAD) $(INITRAMFS_ROOT)/head
	cp $(USER_TAIL) $(INITRAMFS_ROOT)/tail
	cp $(USER_TEE) $(INITRAMFS_ROOT)/tee
	cp $(USER_MORE) $(INITRAMFS_ROOT)/more
	cp $(USER_UNAME) $(INITRAMFS_ROOT)/uname
	cp $(USER_HOSTNAME) $(INITRAMFS_ROOT)/hostname
	cp $(USER_UPTIME) $(INITRAMFS_ROOT)/uptime
	cp $(USER_FIND) $(INITRAMFS_ROOT)/find
	cp $(USER_DU) $(INITRAMFS_ROOT)/du
	cp $(USER_DF) $(INITRAMFS_ROOT)/df
	cp $(USER_SORT) $(INITRAMFS_ROOT)/sort
	cp $(USER_UNIQ) $(INITRAMFS_ROOT)/uniq
	cp $(USER_CUT) $(INITRAMFS_ROOT)/cut
	cp $(USER_XARGS) $(INITRAMFS_ROOT)/xargs
	cp $(USER_SEQ) $(INITRAMFS_ROOT)/seq
	cp $(USER_REALPATH) $(INITRAMFS_ROOT)/realpath
	cp $(USER_ID) $(INITRAMFS_ROOT)/id
	cp $(USER_WHOAMI) $(INITRAMFS_ROOT)/whoami
	cp $(USER_READLINK) $(INITRAMFS_ROOT)/readlink
	cp $(USER_CMP) $(INITRAMFS_ROOT)/cmp
	cp $(USER_YES) $(INITRAMFS_ROOT)/yes
	cp $(USER_INSTALL) $(INITRAMFS_ROOT)/install
	cp $(USER_DIFF) $(INITRAMFS_ROOT)/diff
	cp $(USER_TAR) $(INITRAMFS_ROOT)/tar
	cp $(USER_GZIP) $(INITRAMFS_ROOT)/gzip
	cp $(USER_GZIP) $(INITRAMFS_ROOT)/gunzip
	cp $(USER_MINIZIP) $(INITRAMFS_ROOT)/minizip
	cp $(USER_MINIUNZ) $(INITRAMFS_ROOT)/miniunz
	cp $(USER_PATCH) $(INITRAMFS_ROOT)/patch
	cp $(USER_MAKE) $(INITRAMFS_ROOT)/make
	cp $(USER_BYACC) $(INITRAMFS_ROOT)/byacc
	cp $(USER_SED) $(INITRAMFS_ROOT)/sed
	cp $(USER_EXPR) $(INITRAMFS_ROOT)/expr
	cp $(USER_PRINTF) $(INITRAMFS_ROOT)/printf
	cp $(USER_TR) $(INITRAMFS_ROOT)/tr
	cp $(USER_DD) $(INITRAMFS_ROOT)/dd
	cp $(USER_ED) $(INITRAMFS_ROOT)/ed
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/ln
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/sync
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/test
	cp $(USER_POSIXUTILS) "$(INITRAMFS_ROOT)/["
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/cksum
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/sum
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/comm
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/paste
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/join
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/split
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/od
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/hexdump
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/strings
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/file
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/tty
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/stty
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/time
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/timeout
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/nohup
	cp $(USER_POSIXUTILS) $(INITRAMFS_ROOT)/nice
	cp $(USER_STAT) $(INITRAMFS_ROOT)/stat
	cp $(USER_CHMOD) $(INITRAMFS_ROOT)/chmod
	cp $(USER_CP) $(INITRAMFS_ROOT)/cp
	cp $(USER_RM) $(INITRAMFS_ROOT)/rm
	cp $(USER_MKDIR) $(INITRAMFS_ROOT)/mkdir
	cp $(USER_MV) $(INITRAMFS_ROOT)/mv
	cp $(USER_TAP) $(INITRAMFS_ROOT)/tap
	cp $(USER_SVSCAN) $(INITRAMFS_ROOT)/svscan
	cp $(USER_WEBD) $(INITRAMFS_ROOT)/webd
	cp $(USER_PLAYWAV) $(INITRAMFS_ROOT)/playwav
	cp $(USER_HTTPGET) $(INITRAMFS_ROOT)/httpget
	cp $(USER_UDPDNS) $(INITRAMFS_ROOT)/udpdns
	cp $(USER_UDPECHO) $(INITRAMFS_ROOT)/udpecho
	cp $(USER_NETSTAT) $(INITRAMFS_ROOT)/netstat
	cp $(USER_IFCONFIG) $(INITRAMFS_ROOT)/ifconfig
	cp $(USER_ROUTE) $(INITRAMFS_ROOT)/route
	cp $(USER_ARP) $(INITRAMFS_ROOT)/arp
	cp $(USER_PING) $(INITRAMFS_ROOT)/ping
	cp $(USER_HOST) $(INITRAMFS_ROOT)/host
	cp $(USER_NETCHECK) $(INITRAMFS_ROOT)/netcheck
	cp $(USER_NETABI) $(INITRAMFS_ROOT)/netabi
	cp $(USER_TCPSTRESS) $(INITRAMFS_ROOT)/tcpstress
	cp $(USER_TCPPROBE) $(INITRAMFS_ROOT)/tcpprobe
	cp $(USER_SYSABI) $(INITRAMFS_ROOT)/sysabi
	cp $(USER_SPIN) $(INITRAMFS_ROOT)/spin
	cp $(USER_FPDEMO) $(INITRAMFS_ROOT)/fpdemo
	cp $(USER_UI) $(INITRAMFS_ROOT)/ui
	cp $(USER_DESKTOP) $(INITRAMFS_ROOT)/desktop
	cp $(USER_DISPLAYD) $(INITRAMFS_ROOT)/displayd
	cp $(USER_GUI) $(INITRAMFS_ROOT)/gui
	cp $(USER_SURFACEDEMO) $(INITRAMFS_ROOT)/surfacedemo
	cp $(USER_GUI2DEMO) $(INITRAMFS_ROOT)/gui2demo
	cp $(USER_GUIFAIL) $(INITRAMFS_ROOT)/guifail
	cp $(USER_CALC) $(INITRAMFS_ROOT)/calc
	cp $(USER_NOTES) $(INITRAMFS_ROOT)/notes
	cp $(USER_TEXTEDIT) $(INITRAMFS_ROOT)/textedit
	cp $(USER_PAINT) $(INITRAMFS_ROOT)/paint
	cp $(USER_FILEMAN) $(INITRAMFS_ROOT)/fileman
	cp $(USER_POSIXDEMO) $(INITRAMFS_ROOT)/posixdemo
	cp $(USER_THREADSTRESS) $(INITRAMFS_ROOT)/threadstress
	cp $(USER_EXECDEMO) $(INITRAMFS_ROOT)/execdemo
	cp $(USER_FDPROBE) $(INITRAMFS_ROOT)/fdprobe
	cp $(USER_LOCKPROBE) $(INITRAMFS_ROOT)/lockprobe
	cp $(USER_TTYDEMO) $(INITRAMFS_ROOT)/ttydemo
	cp $(USER_JSONDEMO) $(INITRAMFS_ROOT)/jsondemo
	cp $(USER_INIDEMO) $(INITRAMFS_ROOT)/inidemo
	cp $(USER_LINEDEMO) $(INITRAMFS_ROOT)/linedemo
	cp $(USER_SQLITEDEMO) $(INITRAMFS_ROOT)/sqlitedemo
	cp $(USER_ZLIBDEMO) $(INITRAMFS_ROOT)/zlibdemo
	cp $(USER_LUA) $(INITRAMFS_ROOT)/lua
	cp $(USER_UVDEMO) $(INITRAMFS_ROOT)/uvdemo
	cp $(USER_LIBUVDEMO) $(INITRAMFS_ROOT)/libuvdemo
	cp $(USER_NODEPROBE) $(INITRAMFS_ROOT)/nodeprobe
	cp $(USER_LIBCPROBE) $(INITRAMFS_ROOT)/libcprobe
	cp $(USER_TLSPROBE) $(INITRAMFS_ROOT)/tlsprobe
	cp $(USER_CXXPROBE) $(INITRAMFS_ROOT)/cxxprobe
	cp $(USER_CXXSTLPROBE) $(INITRAMFS_ROOT)/cxxstlprobe
	cp $(EXFAT_IMAGE) $(INITRAMFS_ROOT)/srvros.exfat
	$(PYTHON) tools/pack_runtime_elf.py $(INITRAMFS_ROOT)
	tar --format=ustar --owner=0 --group=0 --numeric-owner -C $(INITRAMFS_ROOT) -cf $(INITRAMFS) .

$(IMAGE): $(LIMINE_DIR)/.ready $(KERNEL) $(INITRAMFS) boot/limine.conf
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/boot/limine
	mkdir -p $(ISO_ROOT)/EFI/BOOT
	cp $(KERNEL) $(ISO_ROOT)/boot/srvros.elf
	cp $(INITRAMFS) $(ISO_ROOT)/boot/initramfs.tar
	cp boot/limine.conf $(ISO_ROOT)/boot/limine/limine.conf
	cp $(LIMINE_DIR)/limine-bios.sys $(ISO_ROOT)/boot/limine/
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_ROOT)/boot/limine/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/
	cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	cp $(LIMINE_DIR)/BOOTIA32.EFI $(ISO_ROOT)/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_ROOT) -o $(IMAGE)
	$(LIMINE_TOOL) bios-install $(IMAGE)

$(USB_IMAGE): $(LIMINE_DIR)/.ready $(KERNEL) $(INITRAMFS) $(EXFAT_IMAGE) boot/limine.conf tools/mk_usb_image.py
	$(PYTHON) tools/mk_usb_image.py \
		--output $@ \
		--kernel $(KERNEL) \
		--initramfs $(INITRAMFS) \
		--config boot/limine.conf \
		--bootx64 $(LIMINE_DIR)/BOOTX64.EFI \
		--bootia32 $(LIMINE_DIR)/BOOTIA32.EFI \
		--exfat $(EXFAT_IMAGE)

.PHONY: usb-image
usb-image: $(USB_IMAGE)

.PHONY: hardware-stage hardware-smoke
hardware-stage: $(LIMINE_DIR)/.ready $(KERNEL) $(INITRAMFS)
	$(PYTHON) tools/hardware_boot.py stage

hardware-smoke: hardware-stage test-core
	$(PYTHON) -m unittest discover -s tools -p test_hardware_boot.py
	$(PYTHON) tools/hardware_boot_smoke.py --qemu $(QEMU) --console-check --responsiveness-check

.PHONY: test-xhci
test-xhci: build/tests/xhci-host$(HOST_EXE)
	$< tools/fixtures/apple-05ac-0291-config.bin tools/fixtures/apple-05ac-0262-config.bin tools/fixtures/apple-05ac-0290-config.bin

build/tests/xhci-host$(HOST_EXE): tools/test_xhci.c kernel/src/drivers/xhci.c kernel/src/drivers/xhci_network.inc kernel/src/drivers/usb_ecm.c kernel/src/drivers/trackpad_pointer.c kernel/src/drivers/apple_trackpad.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< kernel/src/drivers/apple_trackpad.c kernel/src/drivers/trackpad_pointer.c kernel/src/drivers/usb_ecm.c -Wl,--gc-sections -o $@

.PHONY: a1466-rehearsal
a1466-rehearsal: $(USB_IMAGE)
	$(PYTHON) tools/a1466_rehearsal.py --qemu $(QEMU)

.PHONY: qemu-a1466
qemu-a1466:
	$(PYTHON) tools/build_qemu_a1466.py

$(A1466_SSDT): ports/qemu-a1466/acpi/a1466-input.asl tools/build_a1466_ssdt.py
	$(PYTHON) tools/build_a1466_ssdt.py --output $@

.PHONY: a1466-emulator-smoke
a1466-emulator-smoke: qemu-a1466 $(A1466_SSDT) $(USB_IMAGE)
	$(PYTHON) tools/a1466_emulator_smoke.py --qemu $(QEMU_A1466) --ssdt $(A1466_SSDT)

.PHONY: presentation-screens
presentation-screens: $(IMAGE) $(EXFAT_IMAGE)
	$(PYTHON) tools/presentation_screens.py --qemu $(QEMU)

.PHONY: run
run: $(IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio -no-reboot

.PHONY: run-net
run-net: $(IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio \
		-netdev user,id=net0,hostfwd=tcp:127.0.0.1:8080-10.0.2.15:80 \
		-device e1000,netdev=net0 \
		-no-reboot

.PHONY: run-ahci-net
run-ahci-net: $(IMAGE) $(EXFAT_IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio \
		-drive if=none,id=exfat,file=$(EXFAT_IMAGE),format=raw \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=exfat,bus=ahci.0 \
		-netdev user,id=net0,hostfwd=tcp:127.0.0.1:8080-10.0.2.15:80 \
		-device e1000,netdev=net0 \
		-no-reboot

.PHONY: run-ahci2-net
run-ahci2-net: $(IMAGE) $(EXFAT_IMAGE) $(SECOND_EXFAT_IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio \
		-drive if=none,id=exfat0,file=$(EXFAT_IMAGE),format=raw \
		-drive if=none,id=exfat1,file=$(SECOND_EXFAT_IMAGE),format=raw \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=exfat0,bus=ahci.0 \
		-device ide-hd,drive=exfat1,bus=ahci.1 \
		-netdev user,id=net0,hostfwd=tcp:127.0.0.1:8080-10.0.2.15:80 \
		-device e1000,netdev=net0 \
		-no-reboot

.PHONY: run-virtio-net
run-virtio-net: $(IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio \
		-netdev user,id=net0,hostfwd=tcp:127.0.0.1:8080-10.0.2.15:80 \
		-device virtio-net-pci,netdev=net0 \
		-no-reboot

.PHONY: debug
debug: $(IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio -no-reboot -s -S

.PHONY: clean
clean:
	rm -rf build/kernel build/userspace build/iso_root $(INITRAMFS_ROOT) $(IMAGE) $(USB_IMAGE) $(INITRAMFS) $(EXFAT_IMAGE) $(SECOND_EXFAT_IMAGE)

.PHONY: distclean
distclean:
	rm -rf build

-include $(KERNEL_DEP) $(USER_INIT_DEP) $(USER_HELLO_DEP) $(USER_CAT_DEP) $(USER_MORE_DEP) $(USER_SH_DEP) $(USER_LS_DEP) $(USER_ECHO_DEP) $(USER_WRITE_DEP) $(USER_WC_DEP) $(USER_CLEAR_DEP) $(USER_PS_DEP) $(USER_KILL_DEP) $(USER_WHICH_DEP) $(USER_ENV_DEP) $(USER_PWD_DEP) $(USER_TRUE_DEP) $(USER_FALSE_DEP) $(USER_SLEEP_DEP) $(USER_DATE_DEP) $(USER_TOUCH_DEP) $(USER_MKTEMP_DEP) $(USER_BASENAME_DEP) $(USER_DIRNAME_DEP) $(USER_GREP_DEP) $(USER_HEAD_DEP) $(USER_TAIL_DEP) $(USER_TEE_DEP) $(USER_UNAME_DEP) $(USER_HOSTNAME_DEP) $(USER_UPTIME_DEP) $(USER_FIND_DEP) $(USER_DU_DEP) $(USER_DF_DEP) $(USER_SORT_DEP) $(USER_UNIQ_DEP) $(USER_CUT_DEP) $(USER_XARGS_DEP) $(USER_SEQ_DEP) $(USER_REALPATH_DEP) $(USER_ID_DEP) $(USER_WHOAMI_DEP) $(USER_READLINK_DEP) $(USER_CMP_DEP) $(USER_YES_DEP) $(USER_INSTALL_DEP) $(USER_DIFF_DEP) $(USER_TAR_DEP) $(USER_GZIP_DEP) $(USER_PATCH_DEP) $(USER_MAKE_DEP) $(USER_SED_DEP) $(USER_EXPR_DEP) $(USER_PRINTF_DEP) $(USER_TR_DEP) $(USER_DD_DEP) $(USER_ED_DEP) $(USER_POSIXUTILS_DEP) $(USER_STAT_DEP) $(USER_CHMOD_DEP) $(USER_CP_DEP) $(USER_RM_DEP) $(USER_MKDIR_DEP) $(USER_MV_DEP) $(USER_TAP_DEP) $(USER_SVSCAN_DEP) $(USER_WEBD_DEP) $(USER_HTTPGET_DEP) $(USER_UDPDNS_DEP) $(USER_UDPECHO_DEP) $(USER_NETSTAT_DEP) $(USER_IFCONFIG_DEP) $(USER_ROUTE_DEP) $(USER_ARP_DEP) $(USER_PING_DEP) $(USER_HOST_DEP) $(USER_NETCHECK_DEP) $(USER_NETABI_DEP) $(USER_TCPSTRESS_DEP) $(USER_TCPPROBE_DEP) $(USER_SYSABI_DEP) $(USER_SPIN_DEP) $(USER_FPDEMO_DEP) $(USER_UI_DEP) $(USER_DESKTOP_DEP) $(USER_DISPLAYD_DEP) $(USER_GUI_DEP) $(USER_SURFACEDEMO_DEP) $(USER_GUI2DEMO_DEP) $(USER_GUIFAIL_DEP) $(USER_CALC_DEP) $(USER_NOTES_DEP) $(USER_TEXTEDIT_DEP) $(USER_PAINT_DEP) $(USER_POSIXDEMO_DEP) $(USER_THREADSTRESS_DEP) $(USER_EXECDEMO_DEP) $(USER_FDPROBE_DEP) $(USER_LOCKPROBE_DEP) $(USER_TTYDEMO_DEP) $(USER_JSONDEMO_DEP) $(USER_INIDEMO_DEP) $(USER_LINEDEMO_DEP) $(USER_SQLITEDEMO_DEP) $(USER_ZLIBDEMO_DEP) $(USER_LUA_DEP) $(USER_UVDEMO_DEP) $(USER_LIBUVDEMO_DEP) $(USER_NODEPROBE_DEP) $(USER_LIBCPROBE_DEP) $(USER_TLSPROBE_DEP) $(USER_CXXPROBE_DEP) $(USER_CXXSTLPROBE_DEP) $(USER_LIB_DEP) $(ZLIB_DEP) $(MINIZIP_DEP) $(MINIUNZ_DEP) $(CJSON_DEP) $(INI_DEP) $(LINENOISE_DEP) $(UV_SRVROS_DEP) $(UV_UPSTREAM_CORE_DEP) $(SQLITE_DEP) $(BYACC_DEP) $(LUA_CORE_DEP)

.PHONY: test-console
test-console: build/tests/console-host$(HOST_EXE)
	$<

build/tests/console-host$(HOST_EXE): tools/test_console.c kernel/src/console.c shared/include/srvros/font5x7.h $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

.PHONY: test-keyboard test-clock
test-keyboard: build/tests/keyboard-host$(HOST_EXE)
	$<
test-clock: build/tests/clock-host$(HOST_EXE)
	$<
build/tests/keyboard-host$(HOST_EXE): tools/test_keyboard.c kernel/src/arch/x86_64/keyboard.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
build/tests/clock-host$(HOST_EXE): tools/test_clock.c kernel/src/arch/x86_64/clock.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

.PHONY: test-storage
test-storage: build/tests/block-host$(HOST_EXE) build/tests/ahci-host$(HOST_EXE)
	build/tests/block-host$(HOST_EXE)
	build/tests/ahci-host$(HOST_EXE)
build/tests/block-host$(HOST_EXE): tools/test_block.c kernel/src/block.c kernel/src/block_claim.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
build/tests/ahci-host$(HOST_EXE): tools/test_ahci.c kernel/src/drivers/ahci.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

.PHONY: test-core
test-core: test-console test-keyboard test-clock test-storage test-xhci

.PHONY: test-latency test-workqueue test-serial
test-latency: build/tests/latency-host$(HOST_EXE)
	$<
test-workqueue: build/tests/workqueue-host$(HOST_EXE)
	$<
test-serial: build/tests/serial-host$(HOST_EXE)
	$<
build/tests/latency-host$(HOST_EXE): tools/test_latency.c kernel/src/latency.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
build/tests/workqueue-host$(HOST_EXE): tools/test_workqueue.c kernel/src/workqueue.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
build/tests/serial-host$(HOST_EXE): tools/test_serial.c kernel/src/arch/x86_64/serial.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-latency test-workqueue test-serial

.PHONY: test-io-scheduler
test-io-scheduler: build/tests/io-scheduler-host$(HOST_EXE)
	$<
build/tests/io-scheduler-host$(HOST_EXE): tools/test_io_scheduler.c kernel/src/scheduler.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-io-scheduler

.PHONY: test-syscall-sleep
test-syscall-sleep: build/tests/syscall-sleep-host$(HOST_EXE)
	$<
build/tests/syscall-sleep-host$(HOST_EXE): tools/test_syscall_sleep.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-syscall-sleep

.PHONY: test-syscall-poll
test-syscall-poll: build/tests/syscall-poll-host$(HOST_EXE)
	$<
build/tests/syscall-poll-host$(HOST_EXE): tools/test_syscall_poll.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-syscall-poll

.PHONY: test-gui-wait
test-gui-wait: build/tests/gui-wait-host$(HOST_EXE)
	$<
build/tests/gui-wait-host$(HOST_EXE): tools/test_gui_wait.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-desktop: test-gui-wait

.PHONY: test-net-worker
test-net-worker: build/tests/net-worker-host$(HOST_EXE)
	$<
build/tests/net-worker-host$(HOST_EXE): tools/test_net_worker.c kernel/src/net.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-net-worker

.PHONY: test-syscall-cancel
test-syscall-cancel: build/tests/syscall-cancel-host$(HOST_EXE)
	$<
build/tests/syscall-cancel-host$(HOST_EXE): tools/test_syscall_cancel.c kernel/src/arch/x86_64/idt.c kernel/src/arch/x86_64/isr_frame.h kernel/src/arch/x86_64/descriptor_tables.h kernel/src/arch/x86_64/irq.h $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-syscall-cancel

.PHONY: test-syscall-io
test-syscall-io: build/tests/syscall-io-host$(HOST_EXE)
	$<
build/tests/syscall-io-host$(HOST_EXE): tools/test_syscall_io.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-syscall-io

.PHONY: storage-service-smoke
storage-service-smoke: $(IMAGE) $(EXFAT_IMAGE) test-core
	$(PYTHON) tools/storage_service_smoke.py --qemu $(QEMU) --boot-wait $(SMOKE_BOOT_WAIT)

.PHONY: test-fpu
test-fpu: build/tests/fpu-host$(HOST_EXE)
	$<
build/tests/fpu-host$(HOST_EXE): tools/test_fpu.c kernel/src/arch/x86_64/fpu.c kernel/src/arch/x86_64/fpu_test_state.h $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -mno-sse -mno-sse2 -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-fpu

.PHONY: test-pmm interrupt-smoke
test-pmm: build/tests/pmm-host$(HOST_EXE)
	$<
build/tests/pmm-host$(HOST_EXE): tools/test_pmm.c kernel/src/memory/pmm.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-pmm
interrupt-smoke: $(IMAGE) $(EXFAT_IMAGE) test-core
	$(PYTHON) tools/interrupt_smoke.py --qemu $(QEMU) --boot-wait $(SMOKE_BOOT_WAIT)

.PHONY: test-usb-storage usb-storage-smoke
test-usb-storage: build/tests/usb-storage-host$(HOST_EXE)
	$<
build/tests/usb-storage-host$(HOST_EXE): tools/test_usb_storage.c kernel/src/drivers/usb_storage.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I kernel/include $< -o $@
test-core: test-usb-storage
usb-storage-smoke: $(IMAGE) test-core
	$(PYTHON) tools/usb_storage_smoke.py --qemu $(QEMU)

.PHONY: test-autotest hardware-test-smoke
test-autotest: build/tests/autotest-host$(HOST_EXE)
	$<
build/tests/autotest-host$(HOST_EXE): tools/test_autotest.c kernel/src/autotest.c kernel/src/arch/x86_64/reboot.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-autotest
hardware-test-smoke: $(IMAGE) test-core
	$(PYTHON) -m unittest discover -s tools -p 'test_hardware*.py'
	$(PYTHON) tools/hardware_test_smoke.py --qemu $(QEMU)

.PHONY: test-linux-inventory
test-linux-inventory:
	$(PYTHON) -m unittest discover -s tools -p 'test_capture_*.py'
test-core: test-linux-inventory

.PHONY: check-docs
check-docs:
	$(PYTHON) tools/check_docs.py

.PHONY: test-exfat-entries
test-exfat-entries: build/tests/exfat-entries-host$(HOST_EXE)
	$<
build/tests/exfat-entries-host$(HOST_EXE): tools/test_exfat_entries.c kernel/src/fs/exfat.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-exfat-entries

.PHONY: test-exfat-hardening
.PHONY: test-workspace-image
build/tests/workspace-image-host$(HOST_EXE): tools/workspace_image_host.c kernel/src/workspace_image.c kernel/src/block_claim.c kernel/src/fs/exfat.c kernel/src/vfs.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -DSRVROS_EXFAT_HOST_TEST -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include tools/workspace_image_host.c kernel/src/fs/exfat.c kernel/src/vfs.c -Wl,--gc-sections -o $@
test-workspace-image: build/tests/workspace-image-host$(HOST_EXE) build/tests/exfat-driver$(HOST_EXE)
	$(PYTHON) tools/test_workspace_image.py

build/tests/exfat-driver$(HOST_EXE): tools/exfat_host_driver.c kernel/src/fs/exfat.c kernel/src/block_claim.c kernel/src/vfs.c kernel/src/autotest_storage.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -DSRVROS_EXFAT_HOST_TEST -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include tools/exfat_host_driver.c kernel/src/fs/exfat.c kernel/src/vfs.c kernel/src/autotest_storage.c -Wl,--gc-sections -o $@
test-exfat-hardening: build/tests/exfat-driver$(HOST_EXE)
	$(PYTHON) tools/exfat_hardening_test.py

.PHONY: test-exfat-crash
test-exfat-crash: build/tests/exfat-driver$(HOST_EXE)
	$(PYTHON) tools/test_exfat_crash.py --driver $<
test-core: test-exfat-crash

.PHONY: test-exfat-scale
test-exfat-scale: build/tests/exfat-driver$(HOST_EXE)
	$(PYTHON) tools/exfat_scale_test.py

.PHONY: test-workspace
test-workspace: build/tests/workspace-host$(HOST_EXE) build/tests/sync-command-host$(HOST_EXE)
	build/tests/workspace-host$(HOST_EXE)
	build/tests/sync-command-host$(HOST_EXE)
build/tests/workspace-host$(HOST_EXE): tools/test_workspace.c kernel/src/workspace.c kernel/src/block_claim.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I kernel/include $< -o $@
build/tests/sync-command-host$(HOST_EXE): tools/test_sync_command.c userspace/posixutils/posixutils.c $(USER_SYSROOT_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -idirafter userspace/lib/include $< -Wl,--gc-sections -o $@
test-core: test-workspace

.PHONY: workspace-smoke
workspace-smoke: $(IMAGE) test-core
	$(PYTHON) tools/workspace_smoke.py

.PHONY: test-workspace-io
test-workspace-io: build/tests/history-io-host$(HOST_EXE) build/tests/ed-host$(HOST_EXE)
	build/tests/history-io-host$(HOST_EXE)
	$(PYTHON) tools/test_ed_io.py
build/tests/history-io-host$(HOST_EXE): tools/test_history_io.c ports/srvros/linenoise.c ports/upstream/linenoise/linenoise.h $(USER_SYSROOT_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -D_DEFAULT_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I ports/upstream/linenoise -idirafter userspace/lib/include $< -Wl,--gc-sections -o $@
build/tests/ed-host$(HOST_EXE): userspace/ed/ed.c
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@
test-core: test-workspace-io

.PHONY: test-process-output
test-process-output: build/tests/process-output-host$(HOST_EXE)
	$<
build/tests/process-output-host$(HOST_EXE): tools/test_process_output.c kernel/src/process.c kernel/src/pty.c kernel/src/pty_model.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include -DSRVROS_PTY_HOST_TEST $< kernel/src/pty.c kernel/src/pty_model.c -Wl,--gc-sections -o $@
test-core: test-process-output

.PHONY: workspace-latency-smoke
workspace-latency-smoke: $(IMAGE) test-core
	$(PYTHON) tools/workspace_latency_smoke.py

# Native desktop foundation; deliberately opt-in, independent of GUI2.
DESKSTUDY_OBJ := build/userspace/deskstudy/deskstudy.c.o build/userspace/deskstudy/scene.c.o
$(USER_DESKSTUDY): $(ZIG) $(USER_CRT0_OBJ) $(DESKSTUDY_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKSTUDY_OBJ) $(USER_LIB_OBJ) -o $@
-include $(DESKSTUDY_OBJ:.o=.d)

.PHONY: test-desktop
test-core: test-desktop
test-desktop: build/tests/desktop-host$(HOST_EXE) build/tests/gui-host$(HOST_EXE) build/tests/apple-trackpad-host$(HOST_EXE)
	build/tests/desktop-host$(HOST_EXE)
	build/tests/gui-host$(HOST_EXE)
	build/tests/apple-trackpad-host$(HOST_EXE)
	build/tests/desktop-scene-host$(HOST_EXE)
	build/tests/desktop-overdraw-host$(HOST_EXE)
	build/tests/desktop-session-host$(HOST_EXE)
	build/tests/deskclient-host$(HOST_EXE)
	build/tests/desk-app-host$(HOST_EXE)
	build/tests/calculator-host$(HOST_EXE)
	build/tests/deskfs-host$(HOST_EXE)
	build/tests/file-job-host$(HOST_EXE)
	build/tests/file-pool-host$(HOST_EXE)
	build/tests/clipboard-host$(HOST_EXE)
	build/tests/tool-clipboard-host$(HOST_EXE)
build/tests/desktop-host$(HOST_EXE): tools/test_desktop.c userspace/deskstudy/scene.c userspace/deskstudy/scene.h userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c userspace/lib/include/srvros/desk_render.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include tools/test_desktop.c userspace/deskstudy/scene.c userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
build/tests/gui-host$(HOST_EXE): tools/test_gui.c kernel/src/gui.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include $< -o $@

build/tests/apple-trackpad-host$(HOST_EXE): tools/test_apple_trackpad.c kernel/src/drivers/apple_trackpad.c kernel/include/srvros/apple_trackpad.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_apple_trackpad.c kernel/src/drivers/apple_trackpad.c -o $@

DESKD_NOTICE_SRC := userspace/deskd/notifications.c userspace/deskd/notification_view.c
DESKD_OBJ := build/userspace/deskd/notifications.c.o build/userspace/deskd/notification_view.c.o build/userspace/deskd/preferences.c.o build/userspace/deskd/menus.c.o build/userspace/deskd/clipboard.c.o build/userspace/deskd/deskd.c.o build/userspace/deskd/scene.c.o build/userspace/deskd/io_trial.c.o
DESKCLIENT_OBJ := build/userspace/deskclient/deskclient.c.o
DESK_WIDGETS_INCLUDE := userspace/widgets/include
DESK_WIDGETS_SRC := $(wildcard userspace/widgets/src/*.c)
DESK_WIDGETS_HEADERS := $(wildcard userspace/widgets/include/srvros/*.h)
DESK_WIDGETS_OBJ := $(DESK_WIDGETS_SRC:%.c=build/%.c.o)
DESK_WIDGETS_A := build/userspace/widgets/libdeskwidgets.a
# Host tests share toolkit objects; production keeps its freestanding archive.
HOST_DESK_WIDGETS_OBJ := $(patsubst userspace/widgets/src/%.c,build/tests/widgets/%.o,$(DESK_WIDGETS_SRC))
HOST_DESK_WIDGETS_A := build/tests/widgets/libdeskwidgets.a
$(HOST_DESK_WIDGETS_A): $(HOST_DESK_WIDGETS_OBJ) Makefile
	@mkdir -p $(@D)
	rm -f $@.tmp
	$(HOST_AR) rcs $@.tmp $(HOST_DESK_WIDGETS_OBJ)
	mv $@.tmp $@
build/tests/widgets/%.o: userspace/widgets/src/%.c Makefile
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) -MMD -MP -c $< -o $@
-include $(HOST_DESK_WIDGETS_OBJ:.o=.d)
.PHONY: widget-library
widget-library: $(DESK_WIDGETS_A)
$(DESKD_OBJ) $(DESKCLIENT_OBJ) $(DESK_WIDGETS_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(DESK_WIDGETS_A): $(ZIG) $(DESK_WIDGETS_OBJ)
	$(AR) rcs $@ $(DESK_WIDGETS_OBJ)
-include $(DESK_WIDGETS_OBJ:.o=.d)
$(USER_DESKD): $(ZIG) $(USER_CRT0_OBJ) $(DESKD_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKD_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
$(USER_DESKCLIENT): $(ZIG) $(USER_CRT0_OBJ) $(DESKCLIENT_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKCLIENT_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
-include $(DESKD_OBJ:.o=.d) $(DESKCLIENT_OBJ:.o=.d)

test-desktop: build/tests/desktop-scene-host$(HOST_EXE) build/tests/desktop-overdraw-host$(HOST_EXE)
build/tests/desktop-overdraw-host$(HOST_EXE): tools/test_desktop_overdraw.c shared/include/srvros/desktop_protocol.h userspace/deskd/scene.c $(DESKD_NOTICE_SRC) userspace/deskd/scene.h userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/deskd/menus.c
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I $(DESK_WIDGETS_INCLUDE) -idirafter userspace/lib/include tools/test_desktop_overdraw.c userspace/deskd/scene.c $(DESKD_NOTICE_SRC) userspace/deskd/menus.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

build/tests/desktop-scene-host$(HOST_EXE): tools/test_desktop_scene.c shared/include/srvros/desktop_protocol.h userspace/deskd/scene.c $(DESKD_NOTICE_SRC) userspace/deskd/scene.h userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/deskd/menus.c
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I $(DESK_WIDGETS_INCLUDE) -idirafter userspace/lib/include tools/test_desktop_scene.c userspace/deskd/scene.c $(DESKD_NOTICE_SRC) userspace/deskd/menus.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

test-desktop: build/tests/desktop-session-host$(HOST_EXE)
build/tests/desktop-session-host$(HOST_EXE): userspace/deskd/scene.c tools/test_desktop_session.c $(DESKD_NOTICE_SRC) userspace/deskd/notifications.h userspace/deskd/notification_view.h userspace/deskd/preferences.c userspace/deskd/preferences.h userspace/lib/src/desk_preferences.c userspace/deskd/deskd.c userspace/deskd/loop_timing.h userspace/deskd/registry.h userspace/deskd/lifecycle_worker.h userspace/deskd/battery_trial.h userspace/deskd/io_trial.h shared/include/srvros/perf_sample.h shared/include/srvros/desktop_trial.h userspace/deskd/scene.h shared/include/srvros/desktop_protocol.h userspace/lib/src/desk_render.c userspace/lib/include/srvros/desk_log.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/deskd/menus.c
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I $(DESK_WIDGETS_INCLUDE) -idirafter userspace/lib/include tools/test_desktop_session.c userspace/deskd/preferences.c userspace/lib/src/desk_preferences.c userspace/deskd/scene.c $(DESKD_NOTICE_SRC) userspace/deskd/menus.c $(HOST_DESK_WIDGETS_A) userspace/deskd/clipboard.c userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@

.PHONY: test-trackpad-capture
.PHONY: test-loop-timing
test-loop-timing:
	$(PYTHON) tools/test_loop_timing.py
test-core: test-loop-timing

test-core: test-trackpad-capture
test-trackpad-capture: build/tests/trackpad-capture-host$(HOST_EXE)
	$<
	$(PYTHON) tools/test_trackpad_capture_smoke.py
build/tests/trackpad-capture-host$(HOST_EXE): tools/test_trackpad_capture.c kernel/src/trackpad_test.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I kernel/include -I shared/include $< -Wl,--gc-sections -o $@

.PHONY: test-pointer
test-core: test-pointer
test-pointer: build/tests/trackpad-pointer-host$(HOST_EXE) build/tests/mouse-input-host$(HOST_EXE)
	build/tests/trackpad-pointer-host$(HOST_EXE)
	build/tests/mouse-input-host$(HOST_EXE)
	build/tests/syscall-mouse-host$(HOST_EXE)

build/tests/trackpad-pointer-host$(HOST_EXE): tools/test_trackpad_pointer.c kernel/src/drivers/trackpad_pointer.c kernel/include/srvros/trackpad_pointer.h kernel/include/srvros/apple_trackpad.h shared/include/srvros/pointer_config.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include tools/test_trackpad_pointer.c kernel/src/drivers/trackpad_pointer.c -o $@

build/tests/mouse-input-host$(HOST_EXE): tools/test_mouse_input.c kernel/src/drivers/mouse_input.c kernel/include/srvros/mouse.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I kernel/include $< -o $@

test-pointer: build/tests/syscall-mouse-host$(HOST_EXE)
build/tests/syscall-mouse-host$(HOST_EXE): tools/test_syscall_mouse.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

test-desktop: build/tests/deskclient-host$(HOST_EXE)
build/tests/deskclient-host$(HOST_EXE): tools/test_deskclient.c userspace/deskclient/deskclient.c userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c shared/include/srvros/desktop_protocol.h userspace/lib/include/srvros/desk_log.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@

.PHONY: test-widgets
test-desktop: test-widgets
test-widgets: build/tests/widgets-host$(HOST_EXE) build/tests/widget-text-host$(HOST_EXE) build/tests/widget-edit-host$(HOST_EXE) build/tests/widget-ui-host$(HOST_EXE)
	build/tests/widgets-host$(HOST_EXE)
	build/tests/widget-text-host$(HOST_EXE)
	build/tests/widget-edit-host$(HOST_EXE)
	build/tests/widget-ui-host$(HOST_EXE)
build/tests/widgets-host$(HOST_EXE): tools/test_widgets.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c userspace/lib/include/srvros/desk_render.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_widgets.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
build/tests/widget-%-host$(HOST_EXE): tools/test_widget_%.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c userspace/lib/include/srvros/desk_render.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

DESKIO_OBJ := build/userspace/deskio/deskio.c.o
$(USER_DESKIO): $(ZIG) $(USER_CRT0_OBJ) $(DESKIO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKIO_OBJ) $(USER_LIB_OBJ) -o $@
-include $(DESKIO_OBJ:.o=.d)

test-core: test-desktop-io
.PHONY: test-desktop-io
test-desktop-io: build/tests/desktop-io-host$(HOST_EXE) build/tests/desktop-io-state-host$(HOST_EXE)
	build/tests/desktop-io-host$(HOST_EXE)
	build/tests/desktop-io-state-host$(HOST_EXE)
build/tests/desktop-io-host$(HOST_EXE): tools/test_desktop_io.c userspace/deskio/deskio.c shared/include/srvros/desktop_trial.h userspace/lib/include/srvros/trial_file.h
	mkdir -p build/tests
	$(HOST_CC) -D_DEFAULT_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -idirafter userspace/lib/include $< -Wl,--gc-sections -o $@
build/tests/desktop-io-state-host$(HOST_EXE): tools/test_desktop_io_state.c userspace/deskd/io_trial.c userspace/deskd/io_trial.h shared/include/srvros/perf_sample.h shared/include/srvros/desktop_trial.h userspace/lib/include/srvros/desk_log.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -idirafter userspace/lib/include $< -Wl,--gc-sections -o $@

# Optional application services live above the pure widget library. Link this
# archive before widgets and libc; generic userspace programs do not depend on it.
DESK_RUNTIME_SRC := $(wildcard userspace/desktop_runtime/*.c)
DESK_RUNTIME_OBJ := $(DESK_RUNTIME_SRC:%.c=build/%.c.o)
DESK_RUNTIME_A := build/userspace/desktop_runtime/libdeskruntime.a
$(DESK_RUNTIME_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(DESK_RUNTIME_A): $(ZIG) $(DESK_RUNTIME_OBJ)
	$(AR) rcs $@ $(DESK_RUNTIME_OBJ)
-include $(DESK_RUNTIME_OBJ:.o=.d)

.PHONY: test-desktop-chooser
test-desktop: test-desktop-chooser
test-desktop-chooser: build/tests/desktop-chooser-host$(HOST_EXE)
	$<
build/tests/desktop-chooser-host$(HOST_EXE): tools/test_desktop_chooser.c userspace/desktop_runtime/chooser.c userspace/lib/include/srvros/desk_chooser.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I userspace/widgets/include -idirafter userspace/lib/include -I shared/include tools/test_desktop_chooser.c userspace/desktop_runtime/chooser.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

DESKTOOLS_OBJ := $(patsubst %.c,build/%.c.o,$(wildcard userspace/desktools/*.c))
DESKFS_OBJ := build/userspace/deskfs/deskfs.c.o
$(DESKTOOLS_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(USER_DESKTOOLS): $(ZIG) $(USER_CRT0_OBJ) $(DESKTOOLS_OBJ) $(DESK_RUNTIME_A) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKTOOLS_OBJ) $(DESK_RUNTIME_A) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
$(USER_DESKFS): $(ZIG) $(USER_CRT0_OBJ) $(DESKFS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKFS_OBJ) $(USER_LIB_OBJ) -o $@
-include $(DESKTOOLS_OBJ:.o=.d) $(DESKFS_OBJ:.o=.d)

test-desktop: build/tests/desk-app-host$(HOST_EXE) build/tests/calculator-host$(HOST_EXE)
build/tests/desk-app-host$(HOST_EXE): userspace/lib/include/srvros/desk_application.h userspace/lib/include/srvros/desk_connection.h userspace/lib/include/srvros/desk_app.h shared/include/srvros/desktop_protocol.h tools/test_desk_app.c userspace/lib/src/desk_app.c userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include $(filter %.c,$^) -o $@
build/tests/calculator-host$(HOST_EXE): tools/test_calculator.c userspace/desktools/calculator.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $^ -lm -o $@

test-desktop: build/tests/deskfs-host$(HOST_EXE)
build/tests/deskfs-host$(HOST_EXE): tools/test_deskfs.c userspace/deskfs/deskfs.c shared/include/srvros/desktop_file.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include $< -Wl,--gc-sections -o $@

.PHONY: test-syscall-replace
test-core: test-syscall-replace
test-syscall-replace: build/tests/syscall-replace-host$(HOST_EXE)
	build/tests/syscall-replace-host$(HOST_EXE)
build/tests/syscall-replace-host$(HOST_EXE): tools/test_syscall_replace.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

test-desktop: build/tests/file-job-host$(HOST_EXE)
build/tests/file-job-host$(HOST_EXE): tools/test_file_job.c userspace/lib/src/desk_file_job.c userspace/lib/include/srvros/desk_file_job.h shared/include/srvros/desktop_file.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include $< -Wl,--gc-sections -o $@

test-desktop: build/tests/file-pool-host$(HOST_EXE)
build/tests/file-pool-host$(HOST_EXE): tools/test_file_pool.c userspace/lib/src/desk_file_job.c userspace/lib/include/srvros/desk_file_job.h shared/include/srvros/desktop_file.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include $< -Wl,--gc-sections -o $@

test-desktop: build/tests/clipboard-host$(HOST_EXE)
build/tests/clipboard-host$(HOST_EXE): tools/test_clipboard.c userspace/deskd/clipboard.c userspace/lib/src/desk_clipboard.c userspace/deskd/clipboard.h userspace/lib/include/srvros/desk_clipboard.h shared/include/srvros/desktop_protocol.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include tools/test_clipboard.c userspace/deskd/clipboard.c userspace/lib/src/desk_clipboard.c -o $@

test-desktop: build/tests/tool-clipboard-host$(HOST_EXE)
build/tests/tool-clipboard-host$(HOST_EXE): tools/test_tool_clipboard.c userspace/desktools/app.h userspace/desktools/clipboard.c userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_tool_clipboard.c userspace/desktools/clipboard.c userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

.PHONY: test-posix-poll
test-core: test-posix-poll
test-posix-poll: build/tests/posix-poll-host$(HOST_EXE)
	$<
build/tests/posix-poll-host$(HOST_EXE): tools/test_posix_poll.c userspace/lib/src/posix_poll.c userspace/lib/include/srvros/sys.h shared/include/srvros/syscall_numbers.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include $< -o $@

.PHONY: test-posix-dir
test-core: test-posix-dir
test-posix-dir: build/tests/posix-dir-host$(HOST_EXE)
	build/tests/posix-dir-host$(HOST_EXE)
build/tests/posix-dir-host$(HOST_EXE): tools/test_posix_dir.c userspace/lib/src/posix_dir.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include $< -o $@

.PHONY: test-pty-model
test-core: test-pty-model
test-pty-model: build/tests/pty-model-host$(HOST_EXE)
	build/tests/pty-model-host$(HOST_EXE)
build/tests/pty-model-host$(HOST_EXE): tools/test_pty_model.c kernel/src/pty_model.c kernel/include/srvros/pty_model.h shared/include/srvros/syscall_numbers.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include tools/test_pty_model.c kernel/src/pty_model.c -o $@

PTYPROBE_OBJ := build/userspace/ptyprobe/ptyprobe.c.o
$(USER_PTYPROBE): $(ZIG) $(USER_CRT0_OBJ) $(PTYPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(PTYPROBE_OBJ) $(USER_LIB_OBJ) -o $@
-include $(PTYPROBE_OBJ:.o=.d)

.PHONY: test-pty
test-core: test-pty
test-pty: build/tests/pty-host$(HOST_EXE)
	$<
build/tests/pty-host$(HOST_EXE): tools/test_pty.c kernel/src/pty.c kernel/src/pty_model.c kernel/include/srvros/pty.h kernel/include/srvros/pty_model.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -DSRVROS_PTY_HOST_TEST -I kernel/include -I shared/include tools/test_pty.c kernel/src/pty.c kernel/src/pty_model.c -o $@

.PHONY: test-terminal
test-widgets: test-terminal
test-terminal: build/tests/terminal-host$(HOST_EXE)
	build/tests/terminal-host$(HOST_EXE)
	build/tests/terminal-view-host$(HOST_EXE)
build/tests/terminal-host$(HOST_EXE): tools/test_terminal.c userspace/widgets/src/terminal.c userspace/widgets/include/srvros/desk_terminal.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I userspace/widgets/include tools/test_terminal.c userspace/widgets/src/terminal.c -o $@

test-terminal: build/tests/terminal-view-host$(HOST_EXE)
build/tests/terminal-view-host$(HOST_EXE): tools/test_terminal_view.c userspace/widgets/src/terminal_view.c userspace/widgets/src/terminal.c userspace/widgets/src/widgets.c userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I userspace/widgets/include -idirafter userspace/lib/include -I shared/include tools/test_terminal_view.c userspace/widgets/src/terminal_view.c userspace/widgets/src/terminal.c userspace/widgets/src/widgets.c userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

DESKTERM_OBJ := build/userspace/deskterm/deskterm.c.o
$(DESKTERM_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(USER_DESKTERM): $(ZIG) $(USER_CRT0_OBJ) $(DESKTERM_OBJ) $(DESK_RUNTIME_A) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKTERM_OBJ) $(DESK_RUNTIME_A) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
-include $(DESKTERM_OBJ:.o=.d)

.PHONY: test-conio-terminal
test-core: test-conio-terminal
test-conio-terminal: build/tests/conio-terminal-host$(HOST_EXE)
	$<
build/tests/conio-terminal-host$(HOST_EXE): tools/test_conio_terminal.c userspace/lib/src/conio.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include $^ -Wl,--gc-sections -o $@

.PHONY: test-line-terminal
test-core: test-line-terminal
test-line-terminal: build/tests/line-terminal-host$(HOST_EXE)
	$<
build/tests/line-terminal-host$(HOST_EXE): tools/test_line_terminal.c ports/srvros/linenoise.c userspace/widgets/src/terminal.c userspace/widgets/include/srvros/desk_terminal.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I ports/upstream/linenoise -I userspace/widgets/include -idirafter userspace/lib/include -I shared/include $< userspace/widgets/src/terminal.c -Wl,--gc-sections -o $@

.PHONY: test-wait-ownership
test-core: test-wait-ownership
test-wait-ownership: build/tests/wait-ownership-host$(HOST_EXE)
	$<
build/tests/wait-ownership-host$(HOST_EXE): tools/test_wait_ownership.c kernel/src/process.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

.PHONY: test-menu
test-widgets: test-menu
test-menu: build/tests/menu-host$(HOST_EXE)
	build/tests/menu-host$(HOST_EXE)
	build/tests/menu-transport-host$(HOST_EXE)
build/tests/menu-host$(HOST_EXE): tools/test_menu.c userspace/widgets/src/desk_menu.c userspace/widgets/src/widgets.c userspace/widgets/src/text.c userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c $(DESK_WIDGETS_HEADERS) shared/include/srvros/desktop_menu.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I userspace/widgets/include -idirafter userspace/lib/include -I shared/include tools/test_menu.c userspace/widgets/src/desk_menu.c userspace/widgets/src/widgets.c userspace/widgets/src/text.c userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

test-menu: build/tests/menu-transport-host$(HOST_EXE)
build/tests/menu-transport-host$(HOST_EXE): tools/test_menu_transport.c userspace/deskd/menus.c userspace/deskd/menus.h userspace/widgets/src/desk_menu.c userspace/widgets/src/text.c shared/include/srvros/desktop_protocol.h shared/include/srvros/desktop_menu.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I userspace/widgets/include -idirafter userspace/lib/include -I shared/include tools/test_menu_transport.c userspace/deskd/menus.c userspace/widgets/src/desk_menu.c userspace/widgets/src/text.c -Wl,--gc-sections -o $@

.PHONY: test-dialog
test-widgets: test-dialog
test-dialog: build/tests/dialog-host$(HOST_EXE)
	$<
build/tests/dialog-host$(HOST_EXE): tools/test_dialog.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I userspace/widgets/include -idirafter userspace/lib/include -I shared/include tools/test_dialog.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

.PHONY: test-file-picker
test-widgets: test-file-picker
test-file-picker: build/tests/file-picker-host$(HOST_EXE)
	build/tests/file-picker-host$(HOST_EXE)
	build/tests/tool-picker-host$(HOST_EXE)
build/tests/file-picker-host$(HOST_EXE): tools/test_file_picker.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I userspace/widgets/include -idirafter userspace/lib/include -I shared/include tools/test_file_picker.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

test-file-picker: build/tests/tool-picker-host$(HOST_EXE)
build/tests/tool-picker-host$(HOST_EXE): userspace/lib/include/srvros/desk_chooser.h tools/test_tool_picker.c userspace/desktop_runtime/chooser.c userspace/desktools/picker.c userspace/desktools/files_app.c userspace/desktools/app.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I userspace/widgets/include -idirafter userspace/lib/include -I shared/include tools/test_tool_picker.c userspace/desktop_runtime/chooser.c userspace/desktools/picker.c userspace/desktools/files_app.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@

.PHONY: test-rtc
test-rtc: build/tests/rtc-host$(HOST_EXE)
	$<
build/tests/rtc-host$(HOST_EXE): tools/test_rtc.c kernel/src/arch/x86_64/rtc.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-rtc

CLOCKPROBE_OBJ := build/userspace/clockprobe/clockprobe.c.o
$(USER_CLOCKPROBE): $(ZIG) $(USER_CRT0_OBJ) $(CLOCKPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(CLOCKPROBE_OBJ) $(USER_LIB_OBJ) -o $@
-include $(CLOCKPROBE_OBJ:.o=.d)

.PHONY: test-check
test-check: build/tests/check-host$(HOST_EXE)
	$<
build/tests/check-host$(HOST_EXE): tools/test_check.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include tools/test_check.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-widgets: test-check

.PHONY: test-preferences
test-preferences: build/tests/preferences-host$(HOST_EXE)
	$<
build/tests/preferences-host$(HOST_EXE): tools/test_preferences.c userspace/deskd/preferences.c userspace/deskd/preferences.h userspace/lib/src/desk_preferences.c userspace/lib/include/srvros/desk_preferences.h shared/include/srvros/desktop_preferences.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include tools/test_preferences.c userspace/deskd/preferences.c userspace/lib/src/desk_preferences.c -o $@
test-desktop: test-preferences

.PHONY: test-settings
test-settings: build/tests/settings-host$(HOST_EXE)
	$<
build/tests/settings-host$(HOST_EXE): tools/test_settings.c userspace/desktools/settings.c userspace/desktools/app.h userspace/lib/src/desk_preferences.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include tools/test_settings.c userspace/lib/src/desk_preferences.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-desktop: test-settings

.PHONY: test-runtime-elf
test-runtime-elf: $(EXFAT_IMAGE)
	$(PYTHON) tools/test_runtime_elf.py

.PHONY: test-vfs-time
test-vfs-time: build/tests/vfs-time-host$(HOST_EXE)
	$<
build/tests/vfs-time-host$(HOST_EXE): tools/test_vfs_time.c kernel/src/vfs.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include $< -Wl,--gc-sections -o $@
test-core: test-vfs-time

.PHONY: test-table
test-table: build/tests/table-host$(HOST_EXE)
	$<
build/tests/table-host$(HOST_EXE): tools/test_table.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include tools/test_table.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-widgets: test-table

.PHONY: test-system
test-system: build/tests/system-host$(HOST_EXE)
	$<
build/tests/system-host$(HOST_EXE): tools/test_system.c userspace/desktools/system.c userspace/desktools/app.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include tools/test_system.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-desktop: test-system

.PHONY: test-prompt
test-prompt: build/tests/prompt-host$(HOST_EXE)
	$<
build/tests/prompt-host$(HOST_EXE): tools/test_prompt.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include tools/test_prompt.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-widgets: test-prompt

.PHONY: test-file-change
test-file-change: build/tests/file-change-host$(HOST_EXE)
	$<
build/tests/file-change-host$(HOST_EXE): tools/test_file_change.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-file-change

.PHONY: test-files
test-files: build/tests/files-host$(HOST_EXE)
	$<
build/tests/files-host$(HOST_EXE): tools/test_files.c userspace/desktools/files_app.c userspace/desktools/app.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) shared/include/srvros/desktop_file.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include tools/test_files.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-desktop: test-files

.PHONY: test-strftime
test-strftime: build/tests/strftime-host$(HOST_EXE)
	$<
build/tests/strftime-host$(HOST_EXE): tools/test_strftime.c userspace/lib/src/posix_strftime.c
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@
test-core: test-strftime

.PHONY: test-palette
test-palette: build/tests/palette-host$(HOST_EXE)
	$<
build/tests/palette-host$(HOST_EXE): tools/test_palette.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include tools/test_palette.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-widgets: test-palette

.PHONY: test-search
test-search: build/tests/search-host$(HOST_EXE) build/tests/tool-search-host$(HOST_EXE)
	build/tests/search-host$(HOST_EXE)
	build/tests/tool-search-host$(HOST_EXE)
build/tests/search-host$(HOST_EXE): tools/test_search.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
build/tests/tool-search-host$(HOST_EXE): tools/test_tool_search.c userspace/desktools/search.c userspace/desktools/app.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-widgets: test-search

.PHONY: test-calendar
test-calendar: build/tests/calendar-host$(HOST_EXE)
	$<
build/tests/calendar-host$(HOST_EXE): tools/test_calendar.c userspace/lib/src/posix_calendar.c shared/include/srvros/calendar.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include $< -o $@
test-core: test-calendar

.PHONY: test-calendar-widget
test-calendar-widget: build/tests/calendar-widget-host$(HOST_EXE)
	$<
build/tests/calendar-widget-host$(HOST_EXE): tools/test_calendar_widget.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) shared/include/srvros/calendar.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-widgets: test-calendar-widget

.PHONY: test-text-history
test-widgets: test-text-history
test-text-history: build/tests/text-history-host$(HOST_EXE)
	$<
build/tests/text-history-host$(HOST_EXE): tools/test_text_history.c userspace/widgets/src/text.c userspace/widgets/include/srvros/desk_text.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I userspace/widgets/include tools/test_text_history.c userspace/widgets/src/text.c -o $@

.PHONY: test-tabs
test-widgets: test-tabs
test-tabs: build/tests/tabs-host$(HOST_EXE)
	$<
build/tests/tabs-host$(HOST_EXE): tools/test_tabs.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_tabs.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

.PHONY: test-number
test-core: test-number
test-number: build/tests/number-host$(HOST_EXE)
	$<
build/tests/number-host$(HOST_EXE): tools/test_number.c userspace/lib/src/posix_number.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tools/test_number.c -lm -o $@

.PHONY: test-calculator-app
test-desktop: test-calculator-app
test-calculator-app: build/tests/calculator-app-host$(HOST_EXE)
	$<
build/tests/calculator-app-host$(HOST_EXE): tools/test_calculator_app.c userspace/desktools/calculator_app.c userspace/desktools/calculator.c userspace/desktools/app.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_calculator_app.c userspace/desktools/calculator.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -lm -o $@

.PHONY: test-image-view
test-widgets: test-image-view
test-image-view: build/tests/image-view-host$(HOST_EXE)
	$<
build/tests/image-view-host$(HOST_EXE): tools/test_image_view.c tools/image_view_reference.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_image_view.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

IMAGEWORKER_OBJ := build/userspace/imageworker/imageworker.c.o build/userspace/imageworker/decode.c.o
$(USER_IMAGEWORKER): $(ZIG) $(USER_CRT0_OBJ) $(IMAGEWORKER_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(IMAGEWORKER_OBJ) $(USER_LIB_OBJ) -o $@
-include $(IMAGEWORKER_OBJ:.o=.d)

.PHONY: test-image-decode
test-image-decode: build/tests/image-decode-host$(HOST_EXE)
	build/tests/image-decode-host$(HOST_EXE)
build/tests/image-decode-host$(HOST_EXE): tools/test_image_decode.c userspace/imageworker/decode.c userspace/imageworker/decode.h shared/include/srvros/desktop_image.h shared/include/srvros/image_limits.h ports/upstream/stb/stb_image.h
	@mkdir -p $(@D)
	$(HOST_CC) -D_DEFAULT_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -I shared/include tools/test_image_decode.c userspace/imageworker/decode.c -o $@

.PHONY: test-image-job
test-image-job: build/tests/image-job-host$(HOST_EXE)
	build/tests/image-job-host$(HOST_EXE)
build/tests/image-job-host$(HOST_EXE): tools/test_image_job.c userspace/lib/src/desk_image_job.c userspace/lib/include/srvros/desk_image_job.h shared/include/srvros/desktop_image.h
	@mkdir -p $(@D)
	$(HOST_CC) -D_DEFAULT_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -idirafter userspace/lib/include $< -o $@

DESKVIEW_OBJ := build/userspace/deskview/deskview.c.o
$(DESKVIEW_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(USER_DESKVIEW): $(ZIG) $(USER_CRT0_OBJ) $(DESKVIEW_OBJ) $(DESK_RUNTIME_A) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKVIEW_OBJ) $(DESK_RUNTIME_A) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
-include $(DESKVIEW_OBJ:.o=.d)
test-desktop: test-image-view test-image-decode test-image-job

.PHONY: test-preview-app
test-preview-app: build/tests/preview-app-host$(HOST_EXE)
	$<
build/tests/preview-app-host$(HOST_EXE): userspace/desktop_runtime/commands.c userspace/lib/include/srvros/desk_commands.h userspace/lib/include/srvros/desk_runtime.h userspace/lib/include/srvros/desk_chooser.h userspace/desktop_runtime/chooser.c tools/test_preview_app.c userspace/deskview/deskview.c userspace/lib/src/desk_image_job.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -D_DEFAULT_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include tools/test_preview_app.c userspace/desktop_runtime/commands.c userspace/desktop_runtime/chooser.c userspace/lib/src/desk_image_job.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@
test-desktop: test-preview-app

.PHONY: test-image-worker
test-image-worker: build/tests/image-worker-host$(HOST_EXE)
	$<
build/tests/image-worker-host$(HOST_EXE): tools/test_image_worker.c userspace/imageworker/imageworker.c userspace/imageworker/decode.c userspace/imageworker/decode.h ports/upstream/stb/stb_image.h
	@mkdir -p $(@D)
	$(HOST_CC) -D_DEFAULT_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include tools/test_image_worker.c userspace/imageworker/decode.c -Wl,--gc-sections -o $@
test-desktop: test-image-worker

.PHONY: test-document-view
test-document-view: build/tests/document-view-host$(HOST_EXE)
	$<
build/tests/document-view-host$(HOST_EXE): tools/test_document_view.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-desktop: test-document-view

DESKHELP_OBJ := build/userspace/deskhelp/deskhelp.c.o build/userspace/deskhelp/content.c.o
$(DESKHELP_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(USER_DESKHELP): $(ZIG) $(USER_CRT0_OBJ) $(DESKHELP_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKHELP_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
-include $(DESKHELP_OBJ:.o=.d)

.PHONY: test-help-app
test-help-app: build/tests/help-app-host$(HOST_EXE)
	$<
build/tests/help-app-host$(HOST_EXE): tools/test_help_app.c userspace/deskhelp/deskhelp.c userspace/deskhelp/content.c userspace/deskhelp/content.h userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -D_DEFAULT_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include tools/test_help_app.c userspace/deskhelp/content.c userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@
test-desktop: test-help-app

.PHONY: bench-image-view
bench-image-view: build/tests/image-view-bench$(HOST_EXE)
	$<
build/tests/image-view-bench$(HOST_EXE): tools/bench_image_view.c tools/image_view_reference.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I userspace/widgets/include $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@

.PHONY: test-notices test-time-model
test-notices: build/tests/notices-host$(HOST_EXE)
	$<
build/tests/notices-host$(HOST_EXE): tools/test_notices.c $(DESKD_NOTICE_SRC) userspace/deskd/notifications.h userspace/deskd/notification_view.h userspace/lib/src/desk_notify.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS) shared/include/srvros/desktop_notice.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_notices.c $(DESKD_NOTICE_SRC) userspace/lib/src/desk_notify.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-time-model: build/tests/time-model-host$(HOST_EXE)
	$<
build/tests/time-model-host$(HOST_EXE): tools/test_time_model.c userspace/widgets/src/time_model.c userspace/widgets/include/srvros/desk_time_model.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I $(DESK_WIDGETS_INCLUDE) tools/test_time_model.c userspace/widgets/src/time_model.c -o $@
test-desktop: test-notices test-time-model

DESKCLOCK_OBJ := build/userspace/deskclock/deskclock.c.o
$(DESKCLOCK_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(USER_DESKCLOCK): $(ZIG) $(USER_CRT0_OBJ) $(DESKCLOCK_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKCLOCK_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
-include $(DESKCLOCK_OBJ:.o=.d)

.PHONY: test-clock-app
test-clock-app: build/tests/clock-app-host$(HOST_EXE)
	$<
build/tests/clock-app-host$(HOST_EXE): tools/test_clock_app.c userspace/deskclock/deskclock.c userspace/lib/src/desk_notify.c userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_clock_app.c userspace/lib/src/desk_notify.c userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@
test-desktop: test-clock-app

.PHONY: test-popup
test-popup: build/tests/popup-host$(HOST_EXE)
	$<
build/tests/popup-host$(HOST_EXE): tools/test_popup.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-desktop: test-popup

.PHONY: test-tool-context
test-tool-context: build/tests/tool-context-host$(HOST_EXE)
	$<
build/tests/tool-context-host$(HOST_EXE): tools/test_tool_context.c userspace/desktools/menus.c userspace/desktools/app.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@
test-desktop: test-tool-context

.PHONY: test-properties
test-properties: build/tests/properties-host$(HOST_EXE)
	$<
build/tests/properties-host$(HOST_EXE): tools/test_properties.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-desktop: test-properties

.PHONY: test-file-info
test-file-info: build/tests/file-info-host$(HOST_EXE)
	$<
build/tests/file-info-host$(HOST_EXE): tools/test_file_info.c userspace/desktools/file_info.c userspace/desktools/app.h userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) $< userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@
test-desktop: test-file-info

SHOTWORKER_OBJ := build/userspace/shotworker/shotworker.c.o build/userspace/shotworker/save.c.o
$(USER_SHOTWORKER): $(ZIG) $(USER_CRT0_OBJ) $(SHOTWORKER_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(SHOTWORKER_OBJ) $(USER_LIB_OBJ) -o $@
-include $(SHOTWORKER_OBJ:.o=.d)

.PHONY: test-capture test-bmp
test-bmp: build/tests/bmp-host$(HOST_EXE)
	$<
build/tests/bmp-host$(HOST_EXE): tools/test_bmp.c userspace/lib/src/bmp.c userspace/lib/include/srvros/bmp.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include $< userspace/lib/src/bmp.c -o $@
test-capture: build/tests/capture-job-host$(HOST_EXE) build/tests/capture-save-host$(HOST_EXE) test-bmp
	build/tests/capture-job-host$(HOST_EXE)
	build/tests/capture-save-host$(HOST_EXE)
build/tests/capture-job-host$(HOST_EXE): tools/test_capture_job.c userspace/lib/src/desk_capture_job.c userspace/lib/include/srvros/desk_capture_job.h shared/include/srvros/desktop_capture.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -idirafter userspace/lib/include $< -o $@
build/tests/capture-save-host$(HOST_EXE): tools/test_capture_save.c userspace/shotworker/save.c userspace/shotworker/save.h shared/include/srvros/desktop_capture.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -idirafter userspace/lib/include $< -o $@
test-desktop: test-capture

.PHONY: test-slider test-color-panel
test-slider: build/tests/slider-host$(HOST_EXE)
	$<
build/tests/slider-host$(HOST_EXE): tools/test_slider.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I $(DESK_WIDGETS_INCLUDE) -idirafter userspace/lib/include $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-color-panel: build/tests/color-panel-host$(HOST_EXE)
	$<
build/tests/color-panel-host$(HOST_EXE): tools/test_color_panel.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I $(DESK_WIDGETS_INCLUDE) -idirafter userspace/lib/include $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-desktop: test-slider test-color-panel

.PHONY: test-bitmap test-paint
test-bitmap: build/tests/bitmap-host$(HOST_EXE)
	$<
build/tests/bitmap-host$(HOST_EXE): tools/test_bitmap.c userspace/widgets/src/bitmap.c userspace/widgets/include/srvros/desk_bitmap.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I $(DESK_WIDGETS_INCLUDE) $< userspace/widgets/src/bitmap.c -o $@
test-paint: build/tests/paint-host$(HOST_EXE)
	$<
build/tests/paint-host$(HOST_EXE): tools/test_paint.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I $(DESK_WIDGETS_INCLUDE) -idirafter userspace/lib/include $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -o $@
test-desktop: test-bitmap test-paint

DESKSKETCH_OBJ := build/userspace/desksketch/desksketch.c.o
$(DESKSKETCH_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(USER_DESKSKETCH): $(ZIG) $(USER_CRT0_OBJ) $(DESKSKETCH_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKSKETCH_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
-include $(DESKSKETCH_OBJ:.o=.d)

.PHONY: test-sketch-app
test-sketch-app: build/tests/sketch-app-host$(HOST_EXE)
	$<
build/tests/sketch-app-host$(HOST_EXE): tools/test_sketch_app.c userspace/desksketch/desksketch.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I $(DESK_WIDGETS_INCLUDE) -idirafter userspace/lib/include $< $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@
test-desktop: test-sketch-app

.PHONY: test-file-crc
test-file-crc: build/tests/file-crc-host$(HOST_EXE)
	$<
build/tests/file-crc-host$(HOST_EXE): tools/test_file_crc.c shared/include/srvros/desktop_file.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include $< -o $@
test-desktop: test-file-crc

.PHONY: test-text-replace
test-text-replace: build/tests/text-replace-host$(HOST_EXE)
	$<
build/tests/text-replace-host$(HOST_EXE): tools/test_text_replace.c userspace/widgets/src/text.c userspace/widgets/src/text_search.c userspace/widgets/include/srvros/desk_text.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I userspace/widgets/include tools/test_text_replace.c userspace/widgets/src/text.c userspace/widgets/src/text_search.c -o $@
test-desktop: test-text-replace

.PHONY: test-e1000-link
test-e1000-link: build/tests/e1000-link-host$(HOST_EXE)
	$<
build/tests/e1000-link-host$(HOST_EXE): tools/test_e1000_link.c kernel/src/drivers/e1000.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include tools/test_e1000_link.c -Wl,--gc-sections -o $@
test-core: test-e1000-link

.PHONY: test-command-job
test-command-job: build/tests/command-job-host$(HOST_EXE)
	$<
build/tests/command-job-host$(HOST_EXE): tools/test_command_job.c userspace/lib/src/desk_command_job.c userspace/lib/include/srvros/desk_command_job.h shared/include/srvros/syscall_numbers.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include tools/test_command_job.c -o $@
test-desktop: test-command-job

DESKNET_OBJ := build/userspace/desknet/desknet.c.o
$(DESKNET_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(USER_DESKNET): $(ZIG) $(USER_CRT0_OBJ) $(DESKNET_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKNET_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
-include $(DESKNET_OBJ:.o=.d)

.PHONY: test-network-app
test-network-app: build/tests/network-app-host$(HOST_EXE)
	$<
build/tests/network-app-host$(HOST_EXE): tools/test_network_app.c userspace/desknet/desknet.c userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_network_app.c userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@
test-desktop: test-network-app

DESKFIND_OBJ := build/userspace/deskfind/deskfind.c.o
FINDFSWORKER_OBJ := build/userspace/deskfind/worker.c.o
$(DESKFIND_OBJ) $(FINDFSWORKER_OBJ): USER_CFLAGS += -I $(DESK_WIDGETS_INCLUDE)
$(USER_DESKFIND): $(ZIG) $(USER_CRT0_OBJ) $(DESKFIND_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DESKFIND_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
$(USER_FINDFSWORKER): $(ZIG) $(USER_CRT0_OBJ) $(FINDFSWORKER_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(FINDFSWORKER_OBJ) $(DESK_WIDGETS_A) $(USER_LIB_OBJ) -o $@
-include $(DESKFIND_OBJ:.o=.d) $(FINDFSWORKER_OBJ:.o=.d)

.PHONY: test-file-search test-find-worker
test-file-search: build/tests/file-search-host$(HOST_EXE)
	$<
build/tests/file-search-host$(HOST_EXE): tools/test_file_search.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) $< $(HOST_DESK_WIDGETS_A) -o $@
test-find-worker: build/tests/find-worker-host$(HOST_EXE)
	$<
build/tests/find-worker-host$(HOST_EXE): tools/test_find_worker.c userspace/deskfind/worker.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) $< $(HOST_DESK_WIDGETS_A) -o $@
test-desktop: test-file-search test-find-worker

.PHONY: test-find-app
test-find-app: build/tests/find-app-host$(HOST_EXE)
	$<
build/tests/find-app-host$(HOST_EXE): tools/test_find_app.c userspace/deskfind/deskfind.c userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_find_app.c userspace/lib/src/desk_clipboard.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@
test-desktop: test-find-app

.PHONY: test-path
test-path: build/tests/path-host$(HOST_EXE)
	$<
build/tests/path-host$(HOST_EXE): tools/test_path.c $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) $< $(HOST_DESK_WIDGETS_A) -o $@
test-desktop: test-path

.PHONY: test-widget-boundary
test-widgets: test-widget-boundary
test-widget-boundary: $(HOST_DESK_WIDGETS_A)
	$(PYTHON) tools/check_widget_boundary.py --nm $(HOST_NM) $(HOST_DESK_WIDGETS_A)

test-core: test-acpi-io-rule
.PHONY: test-acpi-io-rule
test-acpi-io-rule: build/tests/acpi-io-rule-host$(HOST_EXE)
	$<
build/tests/acpi-io-rule-host$(HOST_EXE): tools/test_acpi_io_rule.c kernel/src/drivers/acpi_io_rule.c kernel/include/srvros/acpi_io_rule.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_acpi_io_rule.c kernel/src/drivers/acpi_io_rule.c -o $@

test-core: test-acpi-ec-resources
.PHONY: test-acpi-ec-resources
test-acpi-ec-resources: build/tests/acpi-ec-resources-host$(HOST_EXE)
	$<
build/tests/acpi-ec-resources-host$(HOST_EXE): tools/test_acpi_ec_resources.c kernel/src/drivers/acpi_ec_resources.c kernel/include/srvros/acpi_ec_resources.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_acpi_ec_resources.c kernel/src/drivers/acpi_ec_resources.c -o $@

.PHONY: test-ec-transport
test-core: test-ec-transport
test-ec-transport: build/tests/ec-transport-host$(HOST_EXE)
	$<
build/tests/ec-transport-host$(HOST_EXE): tools/test_ec_transport.c kernel/src/drivers/ec_transport.c kernel/include/srvros/ec_transport.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_ec_transport.c kernel/src/drivers/ec_transport.c -o $@

.PHONY: test-workspace-log
test-core: test-workspace-log
test-workspace-log: build/tests/workspace-log-host$(HOST_EXE) build/tests/bootlog-host$(HOST_EXE)
	build/tests/workspace-log-host$(HOST_EXE)
	build/tests/bootlog-host$(HOST_EXE)
build/tests/workspace-log-host$(HOST_EXE): tools/test_workspace_log.c kernel/src/workspace_log.c kernel/include/srvros/power.h kernel/include/srvros/workspace_log.h kernel/include/srvros/audio.h kernel/include/srvros/thermal.h shared/include/srvros/audio_abi.h shared/include/srvros/thermal_abi.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I kernel/include $< -o $@
build/tests/bootlog-host$(HOST_EXE): tools/test_bootlog.c kernel/src/bootlog.c kernel/include/srvros/bootlog.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I kernel/include $< -o $@

.PHONY: test-ec-smbus
test-core: test-ec-smbus
test-ec-smbus: build/tests/ec-smbus-host$(HOST_EXE)
	$<
build/tests/ec-smbus-host$(HOST_EXE): tools/test_ec_smbus.c kernel/src/drivers/ec_smbus.c kernel/src/drivers/ec_transport.c kernel/include/srvros/ec_smbus.h kernel/include/srvros/ec_transport.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_ec_smbus.c kernel/src/drivers/ec_smbus.c kernel/src/drivers/ec_transport.c -o $@

.PHONY: test-laptop-resources
test-core: test-laptop-resources
test-laptop-resources: build/tests/laptop-resources-host$(HOST_EXE)
	$<
build/tests/laptop-resources-host$(HOST_EXE): tools/test_laptop_resources.c kernel/src/drivers/laptop_resources.c kernel/include/srvros/laptop_probe.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_laptop_resources.c kernel/src/drivers/laptop_resources.c -o $@

.PHONY: test-desktop-host-tools
test-core: test-desktop-host-tools
test-desktop-host-tools:
	$(PYTHON) tools/test_desktop_wait_records.py
	$(PYTHON) tools/test_hardware_boot.py
	$(PYTHON) tools/test_device_checks.py
	$(PYTHON) tools/test_hardware_desktop.py

.PHONY: test-backlight
test-core: test-backlight
test-backlight: build/tests/backlight-host$(HOST_EXE)
	$<
build/tests/backlight-host$(HOST_EXE): tools/test_backlight.c kernel/src/drivers/backlight.c kernel/include/srvros/backlight.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_backlight.c kernel/src/drivers/backlight.c -o $@

.PHONY: test-hda
test-core: test-hda
test-hda: build/tests/hda-host$(HOST_EXE)
	$<
build/tests/hda-host$(HOST_EXE): tools/test_hda.c kernel/src/drivers/hda_core.c kernel/src/drivers/hda_codec.c kernel/include/srvros/hda_core.h kernel/include/srvros/hda_codec.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_hda.c kernel/src/drivers/hda_core.c kernel/src/drivers/hda_codec.c -o $@

.PHONY: test-pci-config
test-core: test-pci-config
test-pci-config: build/tests/pci-config-host$(HOST_EXE)
	$<
build/tests/pci-config-host$(HOST_EXE): tools/test_pci_config.c kernel/src/arch/x86_64/pci.c kernel/include/srvros/pci.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I kernel/include -I shared/include tools/test_pci_config.c -Wl,--gc-sections -o $@

.PHONY: test-devices
test-core: test-devices
test-devices: build/tests/devices-host$(HOST_EXE)
	$<
build/tests/devices-host$(HOST_EXE): tools/test_devices.c kernel/src/drivers/devices.c kernel/include/srvros/devices.h shared/include/srvros/device_abi.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include tools/test_devices.c kernel/src/drivers/devices.c -o $@

UACPI_SRC := $(wildcard ports/upstream/uacpi/source/*.c)
UACPI_HEADERS := $(shell find ports/upstream/uacpi/include -type f)
.PHONY: acpi-namespace
acpi-namespace: build/tools/acpi-namespace$(HOST_EXE)
build/tools/acpi-namespace$(HOST_EXE): tools/acpi_namespace_probe.c kernel/src/drivers/acpi_ec_resources.c kernel/include/srvros/acpi_ec_resources.h kernel/src/drivers/acpi_io_rule.c kernel/include/srvros/acpi_io_rule.h $(UACPI_SRC) $(UACPI_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -I kernel/include -I ports/upstream/uacpi/include -DUACPI_REDUCED_HARDWARE -DUACPI_NATIVE_MMIO $< kernel/src/drivers/acpi_ec_resources.c kernel/src/drivers/acpi_io_rule.c $(UACPI_SRC) -o $@

.PHONY: test-acpi-namespace
test-acpi-namespace: acpi-namespace
	$(PYTHON) tools/test_acpi_namespace.py

.PHONY: test-make-bootstrap
test-make-bootstrap:
	$(PYTHON) -m unittest discover -s tools -p test_make_bootstrap.py
test-core: test-make-bootstrap

build/kernel/ports/%.c.o: ports/%.c $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready $(UACPI_HEADERS) Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I ports/upstream/uacpi/include -DUACPI_REDUCED_HARDWARE -DUACPI_NATIVE_MMIO -c $< -o $@

.PHONY: test-net-device
test-core: test-net-device
test-net-device: build/tests/net-device-host$(HOST_EXE)
	$<
build/tests/net-device-host$(HOST_EXE): tools/test_net_device.c kernel/src/net_device.c kernel/include/srvros/net_device.h shared/include/srvros/syscall_numbers.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include $< -o $@

.PHONY: test-usb-ecm
test-core: test-usb-ecm
test-usb-ecm: build/tests/usb-ecm-host$(HOST_EXE)
	$<
build/tests/usb-ecm-host$(HOST_EXE): tools/test_usb_ecm.c kernel/src/drivers/usb_ecm.c kernel/include/srvros/usb_ecm.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_usb_ecm.c kernel/src/drivers/usb_ecm.c -o $@

.PHONY: test-sbs
test-core: test-sbs
test-sbs: build/tests/sbs-host$(HOST_EXE)
	$<
build/tests/sbs-host$(HOST_EXE): tools/test_sbs.c kernel/src/drivers/sbs_sample.c kernel/src/drivers/ec_smbus.c kernel/src/drivers/ec_transport.c kernel/include/srvros/sbs_sample.h kernel/include/srvros/ec_smbus.h kernel/include/srvros/ec_transport.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_sbs.c kernel/src/drivers/sbs_sample.c kernel/src/drivers/ec_smbus.c kernel/src/drivers/ec_transport.c -o $@

.PHONY: test-thermal
test-core: test-thermal
test-thermal: build/tests/thermal-host$(HOST_EXE)
	$<
build/tests/thermal-host$(HOST_EXE): tools/test_thermal.c kernel/src/drivers/thermal_core.c kernel/include/srvros/thermal.h shared/include/srvros/thermal_abi.h
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include -I kernel/include tools/test_thermal.c kernel/src/drivers/thermal_core.c -o $@

.PHONY: test-process-loader
test-core: test-process-loader
test-process-loader: build/tests/process-loader-host$(HOST_EXE)
	$<
build/tests/process-loader-host$(HOST_EXE): tools/test_process_loader.c kernel/src/process.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

.PHONY: test-aml-boundary
test-core: test-aml-boundary
test-aml-boundary: build/tests/aml-boundary-host$(HOST_EXE)
	$<
build/tests/aml-boundary-host$(HOST_EXE): tools/test_aml_probe_boundary.c ports/srvros/uacpi/kernel/probe.c $(KERNEL_HEADERS) $(UACPI_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DUACPI_REDUCED_HARDWARE -DUACPI_NATIVE_MMIO -I ports/upstream/uacpi/include -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

.PHONY: test-audio-service
test-core: test-audio-service
test-audio-service: build/tests/audio-service-host$(HOST_EXE)
	$<
build/tests/audio-service-host$(HOST_EXE): tools/test_audio_service.c kernel/src/drivers/audio.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

.PHONY: test-smc
test-core: test-smc
test-smc: build/tests/smc-host$(HOST_EXE) build/tests/smc-service-host$(HOST_EXE)
	build/tests/smc-host$(HOST_EXE)
	build/tests/smc-service-host$(HOST_EXE)
build/tests/smc-host$(HOST_EXE): tools/test_smc.c tools/smc_model.h kernel/src/drivers/smc_transport.c kernel/include/srvros/smc_transport.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_smc.c kernel/src/drivers/smc_transport.c -o $@
build/tests/smc-service-host$(HOST_EXE): tools/test_smc_service.c tools/smc_model.h kernel/src/drivers/smc.c kernel/src/drivers/smc_transport.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include tools/test_smc_service.c kernel/src/drivers/smc_transport.c -Wl,--gc-sections -o $@

$(USER_PLAYWAV): $(ZIG) $(USER_CRT0_OBJ) build/userspace/playwav/playwav.c.o build/userspace/playwav/wav.c.o build/userspace/playwav/convert.c.o build/userspace/playwav/io_stress.c.o $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) build/userspace/playwav/playwav.c.o build/userspace/playwav/wav.c.o build/userspace/playwav/convert.c.o build/userspace/playwav/io_stress.c.o $(USER_LIB_OBJ) -o $@
-include build/userspace/playwav/playwav.c.d build/userspace/playwav/wav.c.d build/userspace/playwav/convert.c.d build/userspace/playwav/io_stress.c.d

.PHONY: test-audio-stream
test-core: test-audio-stream
test-audio-stream: build/tests/audio-stream-host$(HOST_EXE)
	$<
build/tests/audio-stream-host$(HOST_EXE): tools/test_audio_stream.c tools/test_audio_service.c kernel/src/drivers/audio.c kernel/src/drivers/audio_stream.inc $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
build/tests/audio-service-host$(HOST_EXE): kernel/src/drivers/audio_stream.inc
.PHONY: test-wav
test-core: test-wav
test-wav: build/tests/wav-host$(HOST_EXE)
	$<
build/tests/wav-host$(HOST_EXE): tools/test_wav.c userspace/playwav/wav.c userspace/playwav/wav.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tools/test_wav.c userspace/playwav/wav.c -o $@
.PHONY: test-pcm-convert
test-core: test-pcm-convert
test-pcm-convert: build/tests/pcm-convert-host$(HOST_EXE)
	$<
build/tests/pcm-convert-host$(HOST_EXE): tools/test_pcm_convert.c userspace/playwav/convert.c userspace/playwav/convert.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tools/test_pcm_convert.c userspace/playwav/convert.c -o $@

.PHONY: test-acpi-work
test-core: test-acpi-work
test-acpi-work: build/tests/acpi-work-host$(HOST_EXE)
	$<
build/tests/acpi-work-host$(HOST_EXE): tools/test_acpi_work_queue.c ports/srvros/uacpi/runtime/work_queue.c ports/srvros/uacpi/runtime/work_queue.h $(UACPI_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I ports/upstream/uacpi/include tools/test_acpi_work_queue.c ports/srvros/uacpi/runtime/work_queue.c -o $@

# Compile-only feasibility check for the future live adapter. These objects are
# never linked into KERNEL_OBJ and never replace the isolated probe objects.
UACPI_RUNTIME_CHECK_SRC := $(UACPI_SRC) ports/srvros/uacpi/runtime/work_queue.c
UACPI_RUNTIME_CHECK_OBJ := $(UACPI_RUNTIME_CHECK_SRC:%.c=build/acpi-runtime-check/%.c.o)
.PHONY: check-acpi-runtime-build
check-acpi-runtime-build: build/acpi-runtime-check/combined.o
build/acpi-runtime-check/combined.o: $(UACPI_RUNTIME_CHECK_OBJ)
	$(LD) -m elf_x86_64 -r $(UACPI_RUNTIME_CHECK_OBJ) -o $@
build/acpi-runtime-check/%.c.o: %.c $(ZIG) $(UACPI_HEADERS) ports/srvros/uacpi/runtime/work_queue.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I ports/upstream/uacpi/include -DUACPI_NATIVE_MMIO -c $< -o $@
-include $(UACPI_RUNTIME_CHECK_OBJ:.o=.d)

.PHONY: test-heap
test-core: test-heap
test-heap: build/tests/heap-host$(HOST_EXE)
	$<
build/tests/heap-host$(HOST_EXE): tools/test_heap.c kernel/src/memory/heap.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include $< -o $@

.PHONY: test-semaphore
test-core: test-semaphore
test-semaphore: build/tests/semaphore-host$(HOST_EXE)
	$<
build/tests/semaphore-host$(HOST_EXE): tools/test_semaphore.c kernel/src/semaphore.c kernel/include/srvros/semaphore.h kernel/include/srvros/scheduler.h kernel/include/srvros/timer.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -I kernel/include $< -o $@

.PHONY: test-irq-handlers
test-core: test-irq-handlers
test-irq-handlers: build/tests/irq-handlers-host$(HOST_EXE)
	$<
build/tests/irq-handlers-host$(HOST_EXE): tools/test_irq_handlers.c kernel/src/irq_handlers.c kernel/include/srvros/irq_handlers.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -I kernel/include $< -o $@

.PHONY: test-ioapic
test-core: test-ioapic
test-ioapic: build/tests/ioapic-host$(HOST_EXE)
	$<
build/tests/ioapic-host$(HOST_EXE): tools/test_ioapic.c kernel/src/arch/x86_64/ioapic.c kernel/src/arch/x86_64/ioapic.h $(KERNEL_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include $< -o $@

.PHONY: test-acpi-worker
test-core: test-acpi-worker
test-acpi-worker: build/tests/acpi-worker-host$(HOST_EXE)
	$<
build/tests/acpi-worker-host$(HOST_EXE): tools/test_acpi_worker.c ports/srvros/uacpi/runtime/work_worker.c ports/srvros/uacpi/runtime/work_worker.h ports/srvros/uacpi/runtime/work_queue.c ports/srvros/uacpi/runtime/work_queue.h $(UACPI_HEADERS) $(KERNEL_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -I kernel/include -I ports/upstream/uacpi/include tools/test_acpi_worker.c ports/srvros/uacpi/runtime/work_queue.c -o $@

test-core: test-sci-owner
test-core: test-ec-query-budget
test-core: test-acpi-event-io
.PHONY: test-acpi-event-io
test-acpi-event-io: build/tests/acpi-event-io-host$(HOST_EXE)
	$<
build/tests/acpi-event-io-host$(HOST_EXE): tools/test_acpi_event_io.c ports/srvros/uacpi/runtime/event_io.c ports/srvros/uacpi/runtime/event_io.h kernel/src/drivers/acpi_ec_resources.c kernel/include/srvros/acpi_ec_resources.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_acpi_event_io.c ports/srvros/uacpi/runtime/event_io.c kernel/src/drivers/acpi_ec_resources.c -o $@

.PHONY: test-ec-query-budget
test-ec-query-budget: build/tests/ec-query-budget-host$(HOST_EXE)
	$<
build/tests/ec-query-budget-host$(HOST_EXE): tools/test_ec_query_budget.c ports/srvros/uacpi/runtime/ec_query_budget.c ports/srvros/uacpi/runtime/ec_query_budget.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tools/test_ec_query_budget.c ports/srvros/uacpi/runtime/ec_query_budget.c -o $@

.PHONY: test-sci-owner
test-sci-owner: build/tests/sci-owner-host$(HOST_EXE)
	$<
build/tests/sci-owner-host$(HOST_EXE): tools/test_sci_owner.c ports/srvros/uacpi/runtime/sci_owner.c ports/srvros/uacpi/runtime/sci_owner.h kernel/include/srvros/acpi.h kernel/include/srvros/irq_handlers.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_sci_owner.c ports/srvros/uacpi/runtime/sci_owner.c -o $@

.PHONY: test-sci-gate
test-core: test-sci-gate
test-sci-gate: build/tests/sci-gate-host$(HOST_EXE)
	$<
build/tests/sci-gate-host$(HOST_EXE): tools/test_sci_gate.c ports/srvros/uacpi/runtime/sci_test.c $(UACPI_HEADERS) $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include -I ports/upstream/uacpi/include $< -Wl,--gc-sections -o $@

# Explicit, separate VM-only runtime kernel. Never substitutes build/kernel/srvros.elf.
UACPI_VM_SRC := $(UACPI_SRC) ports/srvros/uacpi/runtime/qemu_adapter.c ports/srvros/uacpi/runtime/ec_fixture.c ports/srvros/uacpi/runtime/sci_owner.c ports/srvros/uacpi/runtime/ec_query_budget.c ports/srvros/uacpi/runtime/event_io.c
UACPI_VM_OBJ := $(UACPI_VM_SRC:%.c=build/acpi-runtime/%.c.o)
UACPI_WORK_NATIVE_OBJ := $(filter %/runtime/work_queue.c.o %/runtime/work_worker.c.o %/runtime/work_worker_test.c.o %/runtime/sci_test.c.o,$(UACPI_KERNEL_OBJ))
.PHONY: acpi-runtime-vm-kernel
acpi-runtime-vm-kernel: build/acpi-runtime/srvros.elf
build/acpi-runtime/srvros.elf: $(filter-out $(UACPI_KERNEL_OBJ),$(KERNEL_OBJ)) $(UACPI_WORK_NATIVE_OBJ) $(UACPI_VM_OBJ) kernel/linker.ld
	$(LD) -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 --gc-sections -T kernel/linker.ld $(filter-out kernel/linker.ld,$^) -o $@
build/acpi-runtime/%.c.o: %.c $(ZIG) $(UACPI_HEADERS) Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I ports/upstream/uacpi/include -DUACPI_NATIVE_MMIO -c $< -o $@
-include $(UACPI_VM_OBJ:.o=.d)

.PHONY: test-gfx-blit
test-gfx-blit: build/tests/gfx-blit-host$(HOST_EXE)
	$<
build/tests/gfx-blit-host$(HOST_EXE): tools/test_gfx_blit.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-gfx-blit

.PHONY: test-scheduler-status
test-scheduler-status: build/tests/scheduler-status-host$(HOST_EXE)
	$<
build/tests/scheduler-status-host$(HOST_EXE): tools/test_scheduler_status.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@
test-core: test-scheduler-status

.PHONY: test-serial-records
test-serial-records:
	$(PYTHON) -m unittest discover -s tools -p test_serial_records.py
test-core: test-serial-records

.PHONY: test-power
test-core: test-power
test-power: build/tests/power-host$(HOST_EXE) build/tests/power-service-host$(HOST_EXE)
	build/tests/power-host$(HOST_EXE)
	build/tests/power-service-host$(HOST_EXE)
build/tests/power-host$(HOST_EXE): tools/test_power.c kernel/src/drivers/power_core.c kernel/include/srvros/power.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include tools/test_power.c kernel/src/drivers/power_core.c -o $@
build/tests/power-service-host$(HOST_EXE): tools/test_power_service.c kernel/src/drivers/power.c kernel/src/drivers/power_core.c kernel/include/srvros/power.h $(KERNEL_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include tools/test_power_service.c kernel/src/drivers/power_core.c -o $@

.PHONY: test-vm-sockets
test-core: test-vm-sockets
test-vm-sockets:
	$(PYTHON) tools/test_vm_sockets.py

# Explicit battery diagnostic: full uACPI, never the ordinary desktop payload.
UACPI_BATTERY_SRC := $(UACPI_SRC) ports/srvros/uacpi/runtime/battery_event_hold.c ports/srvros/uacpi/runtime/battery_runtime.c ports/srvros/uacpi/runtime/sci_owner.c ports/srvros/uacpi/runtime/ec_query_budget.c ports/srvros/uacpi/runtime/event_io.c
UACPI_BATTERY_OBJ := $(UACPI_BATTERY_SRC:%.c=build/acpi-battery/%.c.o)
.PHONY: acpi-battery-kernel
acpi-battery-kernel: build/acpi-battery/srvros.elf
build/acpi-battery/srvros.elf: $(filter-out $(UACPI_KERNEL_OBJ),$(KERNEL_OBJ)) $(UACPI_WORK_NATIVE_OBJ) $(UACPI_BATTERY_OBJ) build/acpi-battery/ports/srvros/uacpi/runtime/battery_native_platform.c.o kernel/linker.ld
	$(LD) -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 --gc-sections -T kernel/linker.ld $(filter-out kernel/linker.ld,$^) -o $@
build/acpi-battery/%.c.o: %.c $(ZIG) $(UACPI_HEADERS) $(KERNEL_HEADERS) Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I ports/upstream/uacpi/include -DUACPI_NATIVE_MMIO -c $< -o $@
-include $(UACPI_BATTERY_OBJ:.o=.d) build/acpi-battery/ports/srvros/uacpi/runtime/battery_native_platform.c.d
.PHONY: acpi-battery-vm-kernel
acpi-battery-vm-kernel: build/acpi-battery/vm.elf
build/acpi-battery/vm.elf: $(filter-out $(UACPI_KERNEL_OBJ),$(KERNEL_OBJ)) $(UACPI_WORK_NATIVE_OBJ) $(UACPI_BATTERY_OBJ) build/acpi-battery/ports/srvros/uacpi/runtime/battery_vm_platform.c.o kernel/linker.ld
	$(LD) -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 --gc-sections -T kernel/linker.ld $(filter-out kernel/linker.ld,$^) -o $@
-include build/acpi-battery/ports/srvros/uacpi/runtime/battery_vm_platform.c.d
.PHONY: test-battery-native-policy
test-core: test-battery-native-policy
test-battery-native-policy: build/tests/battery-native-policy$(HOST_EXE)
	$<
build/tests/battery-native-policy$(HOST_EXE): tools/test_battery_native_policy.c ports/srvros/uacpi/runtime/battery_native_policy.h ports/srvros/uacpi/runtime/battery_bounded_policy.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@

.PHONY: test-battery-event-hold
test-core: test-battery-event-hold
test-battery-event-hold: build/tests/battery-event-hold$(HOST_EXE)
	$<
build/tests/battery-event-hold$(HOST_EXE): tools/test_battery_event_hold.c ports/srvros/uacpi/runtime/battery_event_hold.c ports/srvros/uacpi/runtime/battery_event_hold.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $(filter %.c,$^) -o $@

.PHONY: test-battery-transition
test-core: test-battery-transition
test-battery-transition: build/tests/battery-transition$(HOST_EXE)
	$<
	$(PYTHON) tools/test_battery_smbus_transition_records.py
build/tests/battery-transition$(HOST_EXE): tools/test_battery_transition.c ports/srvros/uacpi/runtime/battery_transition.h kernel/include/srvros/sbs_sample.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include $< -o $@

.PHONY: test-battery-cache
test-core: test-battery-cache
test-battery-cache: build/tests/battery-cache$(HOST_EXE)
	$<
build/tests/battery-cache$(HOST_EXE): tools/test_battery_cache.c kernel/src/drivers/battery_cache.c kernel/include/srvros/battery_cache.h kernel/include/srvros/battery_service.h kernel/include/srvros/sbs_sample.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include $(filter %.c,$^) -o $@

.PHONY: test-exfat-scratch
test-exfat-scratch: build/tests/exfat-driver$(HOST_EXE)
	$(PYTHON) tools/test_exfat_allocation_check.py
	$(PYTHON) tools/exfat_scratch_test.py
test-core: test-exfat-scratch

.PHONY: test-sci-observe
test-core: test-sci-observe
test-sci-observe: build/tests/sci-observe-host$(HOST_EXE)
	$<
build/tests/sci-observe-host$(HOST_EXE): tools/test_sci_observe.c ports/srvros/uacpi/runtime/sci_observe.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@

.PHONY: test-sci-observe-records
test-core: test-sci-observe-records
test-sci-observe-records:
	$(PYTHON) tools/test_sci_observe_records.py

.PHONY: test-lapic-snapshot
test-core: test-lapic-snapshot
test-lapic-snapshot: build/tests/lapic-snapshot-host$(HOST_EXE)
	$<
build/tests/lapic-snapshot-host$(HOST_EXE): tools/test_lapic_snapshot.c kernel/src/arch/x86_64/lapic.c kernel/src/arch/x86_64/lapic.h $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include $< -o $@

.PHONY: test-irq-completion
test-core: test-irq-completion
test-irq-completion: build/tests/irq-completion-host$(HOST_EXE)
	$<
build/tests/irq-completion-host$(HOST_EXE): tools/test_irq_completion.c kernel/src/irq_handlers.c kernel/src/arch/x86_64/irq.c $(KERNEL_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include $< -o $@

.PHONY: test-ec-query-rearm
test-core: test-ec-query-rearm
test-ec-query-rearm: build/tests/ec-query-rearm-host$(HOST_EXE)
	$<
build/tests/ec-query-rearm-host$(HOST_EXE): tools/test_ec_query_rearm.c ports/srvros/uacpi/runtime/ec_query_rearm.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@

.PHONY: test-battery-desktop-records
test-core: test-battery-desktop-records
test-battery-desktop-records:
	$(PYTHON) tools/test_battery_desktop_records.py

.PHONY: test-battery-view
test-core: test-battery-view
test-battery-view: build/tests/battery-view$(HOST_EXE)
	$<
build/tests/battery-view$(HOST_EXE): tools/test_battery_view.c kernel/src/drivers/battery_cache.c userspace/widgets/src/battery.c userspace/widgets/include/srvros/desk_battery.h kernel/include/srvros/battery_snapshot.h shared/include/srvros/battery_abi.h shared/include/srvros/battery_control_abi.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include -I userspace/widgets/include tools/test_battery_view.c kernel/src/drivers/battery_cache.c userspace/widgets/src/battery.c -o $@

.PHONY: test-battery-control
test-core: test-battery-control
test-battery-control: build/tests/battery-control$(HOST_EXE)
	$<
build/tests/battery-control$(HOST_EXE): tools/test_battery_control.c kernel/src/drivers/battery_control_policy.c kernel/include/srvros/battery_control.h kernel/include/srvros/battery_service.h shared/include/srvros/battery_control_abi.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include tools/test_battery_control.c kernel/src/drivers/battery_control_policy.c -o $@

.PHONY: test-battery-controls-records
test-core: test-battery-controls-records
test-battery-controls-records:
	$(PYTHON) tools/test_battery_controls_records.py
	$(PYTHON) tools/test_battery_workspace_controls_records.py
	$(PYTHON) tools/test_battery_control_files_records.py
	$(PYTHON) tools/test_battery_home_records.py
	$(PYTHON) tools/test_battery_home_desktop_records.py

.PHONY: test-battery-reset-policy
test-core: test-battery-reset-policy
test-battery-reset-policy: build/tests/battery-reset-policy$(HOST_EXE)
	$<
build/tests/battery-reset-policy$(HOST_EXE): tools/test_battery_reset_policy.c ports/srvros/uacpi/runtime/battery_reset_policy.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@

.PHONY: test-battery-recovery-resources
test-core: test-battery-recovery-resources
test-battery-recovery-resources: build/tests/battery-recovery-resources$(HOST_EXE)
	$<
build/tests/battery-recovery-resources$(HOST_EXE): tools/test_battery_recovery_resources.c ports/srvros/uacpi/runtime/battery_recovery_resources.h kernel/include/srvros/ec_transport.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include $< -o $@

.PHONY: test-battery-query
test-core: test-battery-query
test-battery-query: build/tests/battery-query-receipt$(HOST_EXE)
	$<
	$(PYTHON) tools/test_battery_query_records.py
	$(PYTHON) tools/test_battery_native_query_records.py
build/tests/battery-query-receipt$(HOST_EXE): tools/test_battery_query_receipt.c ports/srvros/uacpi/runtime/battery_query_receipt.h ports/srvros/uacpi/runtime/battery_query_policy.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@

.PHONY: test-battery-smbus-records
test-core: test-battery-smbus-records
test-battery-smbus-records:
	$(PYTHON) tools/test_battery_smbus_records.py

.PHONY: test-battery-native-smbus-records
test-core: test-battery-native-smbus-records
test-battery-native-smbus-records:
	$(PYTHON) tools/test_battery_native_smbus_records.py
	$(PYTHON) tools/test_battery_smbus_workspace_records.py
	$(PYTHON) tools/test_battery_smbus_files_records.py

.PHONY: test-battery-event-accounting
test-core: test-battery-event-accounting
test-battery-event-accounting: build/tests/battery-event-accounting$(HOST_EXE)
	$<
	$(PYTHON) tools/test_battery_monitor_records.py
build/tests/battery-event-accounting$(HOST_EXE): tools/test_battery_event_accounting.c ports/srvros/uacpi/runtime/battery_event_accounting.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@

.PHONY: test-process-lifetime
test-core: test-process-lifetime
test-process-lifetime: build/tests/process-lifetime-host$(HOST_EXE)
	$<
build/tests/process-lifetime-host$(HOST_EXE): tools/test_process_lifetime.c kernel/src/process.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

LIFETIMEPROBE_OBJ := build/userspace/lifetimeprobe/lifetimeprobe.c.o
$(USER_LIFETIMEPROBE): $(ZIG) $(USER_CRT0_OBJ) $(LIFETIMEPROBE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(LIFETIMEPROBE_OBJ) $(USER_LIB_OBJ) -o $@
-include $(LIFETIMEPROBE_OBJ:.o=.d)

.PHONY: test-desktop-battery-trial
test-core: test-desktop-battery-trial
test-desktop-battery-trial: build/tests/desktop-battery-trial$(HOST_EXE)
	$<
build/tests/desktop-battery-trial$(HOST_EXE): tools/test_desktop_battery_trial.c userspace/deskd/battery_trial.h shared/include/srvros/battery_abi.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I shared/include $< -o $@

.PHONY: test-battery-preflight
test-core: test-battery-preflight
test-battery-preflight: build/tests/battery-preflight-host$(HOST_EXE)
	$<
build/tests/battery-preflight-host$(HOST_EXE): tools/test_battery_preflight.c kernel/src/drivers/laptop_probe.c kernel/include/srvros/laptop_probe.h $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include tools/test_battery_preflight.c -o $@

# Air6,1 is a separate five-sample diagnostic binary, never a desktop profile.
.PHONY: acpi-air61-battery-kernel
acpi-air61-battery-kernel: build/acpi-air61-battery/srvros.elf
build/acpi-air61-battery/srvros.elf: $(filter-out $(UACPI_KERNEL_OBJ),$(KERNEL_OBJ)) $(UACPI_WORK_NATIVE_OBJ) $(UACPI_BATTERY_OBJ) build/acpi-air61-battery/battery_native_platform.o kernel/linker.ld
	$(LD) -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 --gc-sections -T kernel/linker.ld $(filter-out kernel/linker.ld,$^) -o $@
build/acpi-air61-battery/battery_native_platform.o: ports/srvros/uacpi/runtime/battery_native_platform.c $(ZIG) $(UACPI_HEADERS) $(KERNEL_HEADERS) Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I ports/upstream/uacpi/include -DUACPI_NATIVE_MMIO -DSRVROS_BATTERY_AIR61 -c $< -o $@
-include build/acpi-air61-battery/battery_native_platform.d

.PHONY: test-air61-battery-policy
test-core: test-air61-battery-policy
test-air61-battery-policy: build/tests/air61-battery-policy$(HOST_EXE)
	$<
build/tests/air61-battery-policy$(HOST_EXE): tools/test_battery_native_policy.c ports/srvros/uacpi/runtime/battery_native_policy.h ports/srvros/uacpi/runtime/battery_bounded_policy.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -DSRVROS_BATTERY_AIR61 $< -o $@

.PHONY: test-air61-battery-profile
test-core: test-air61-battery-profile
test-air61-battery-profile: build/tests/air61-battery-profile$(HOST_EXE)
	$<
build/tests/air61-battery-profile$(HOST_EXE): tools/test_air61_battery_profile.c ports/srvros/uacpi/runtime/battery_native_platform.c $(UACPI_HEADERS) $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -ffunction-sections -fdata-sections -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include -I ports/upstream/uacpi/include $< -Wl,--gc-sections -o $@

.PHONY: test-battery-failure-display
test-core: test-battery-failure-display
test-battery-failure-display: build/tests/battery-failure-display$(HOST_EXE)
	$<
build/tests/battery-failure-display$(HOST_EXE): tools/test_battery_failure_display.c ports/srvros/uacpi/runtime/battery_failure_display.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@

.PHONY: test-log-archive
test-core: test-log-archive
test-log-archive:
	$(PYTHON) tools/test_hardware_log_archive.py

.PHONY: acpi-air61-battery-vm-kernel
acpi-air61-battery-vm-kernel: build/acpi-air61-battery/vm.elf
build/acpi-air61-battery/vm.elf: $(filter-out $(UACPI_KERNEL_OBJ),$(KERNEL_OBJ)) $(UACPI_WORK_NATIVE_OBJ) $(UACPI_BATTERY_OBJ) build/acpi-air61-battery/battery_vm_platform.o kernel/linker.ld
	$(LD) -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 --gc-sections -T kernel/linker.ld $(filter-out kernel/linker.ld,$^) -o $@
build/acpi-air61-battery/battery_vm_platform.o: ports/srvros/uacpi/runtime/battery_vm_platform.c $(ZIG) $(UACPI_HEADERS) $(KERNEL_HEADERS) Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I ports/upstream/uacpi/include -DUACPI_NATIVE_MMIO -DSRVROS_BATTERY_AIR61 -c $< -o $@
-include build/acpi-air61-battery/battery_vm_platform.d

.PHONY: test-scheduler-activity
test-scheduler-activity:
	$(PYTHON) tools/test_scheduler_activity.py
test-core: test-scheduler-activity

.PHONY: test-storage-wait-result
test-core: test-storage-wait-result
test-storage-wait-result:
	$(PYTHON) tools/test_storage_wait_result.py

.PHONY: test-tcp-accept
test-core: test-tcp-accept
test-tcp-accept: build/tests/tcp-accept-host$(HOST_EXE)
	$<
build/tests/tcp-accept-host$(HOST_EXE): tools/test_tcp_accept.c kernel/src/net.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

.PHONY: test-e1000-rx
test-core: test-e1000-rx
test-e1000-rx: build/tests/e1000-rx-host$(HOST_EXE)
	$<
build/tests/e1000-rx-host$(HOST_EXE): tools/test_e1000_rx.c kernel/src/drivers/e1000.c $(KERNEL_HEADERS)
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I $(LIMINE_PROTOCOL_DIR)/include $< -Wl,--gc-sections -o $@

.PHONY: test-battery-startup-timing
test-core: test-battery-startup-timing
test-battery-startup-timing: build/tests/battery-startup-timing$(HOST_EXE)
	$<
build/tests/battery-startup-timing$(HOST_EXE): tools/test_battery_startup_timing.c ports/srvros/uacpi/runtime/battery_startup_timing.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror $< -o $@

.PHONY: test-stdio-float
test-core: test-stdio-float
test-stdio-float: build/tests/stdio-float-host$(HOST_EXE)
	$<
build/tests/stdio-float-host$(HOST_EXE): tools/test_stdio_float.c userspace/lib/src/stdio_float.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tools/test_stdio_float.c -lm -o $@

# Private formatter header also participates in userspace incremental builds.
build/userspace/lib/src/posix_stdio.c.o: userspace/lib/src/stdio_float.h

.PHONY: bench-heap-startup
bench-heap-startup: build/tests/heap-startup-bench$(HOST_EXE)
	$<
build/tests/heap-startup-bench$(HOST_EXE): tools/bench_heap_startup.c kernel/src/memory/heap.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include $< -o $@

.PHONY: test-posix-capture
test-core: test-posix-capture
test-posix-capture: build/tests/posix-capture-host$(HOST_EXE) build/tests/block-ram-host$(HOST_EXE)
	build/tests/block-ram-host$(HOST_EXE)
	$<
	$(PYTHON) tools/test_posix_capture_result.py
build/tests/posix-capture-host$(HOST_EXE): tools/test_posix_capture.c kernel/src/posix_capture.c kernel/include/srvros/posix_capture.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include tools/test_posix_capture.c -o $@

build/tests/block-ram-host$(HOST_EXE): tools/test_block_ram.c kernel/src/block_ram.c kernel/include/srvros/block.h $(LIMINE_PROTOCOL_DIR)/.ready
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -I kernel/include -I shared/include -I $(LIMINE_PROTOCOL_DIR)/include tools/test_block_ram.c kernel/src/block_ram.c -o $@

.PHONY: test-audio-io-receipt
test-core: test-audio-io-receipt
test-audio-io-receipt: build/tests/audio-io-receipt$(HOST_EXE)
	$<
build/tests/audio-io-receipt$(HOST_EXE): tools/test_audio_io_receipt.c userspace/playwav/io_stress_receipt.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $< -o $@

.PHONY: test-process-service
test-process-service: build/tests/process-service-host$(HOST_EXE)
	$<
build/tests/process-service-host$(HOST_EXE): tools/test_process_service.c kernel/src/process.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I build/limine-protocol/include $< -Wl,--gc-sections -o $@
test-core: test-process-service

.PHONY: test-graphics-service
test-graphics-service: build/tests/graphics-service-host$(HOST_EXE)
	$<
build/tests/graphics-service-host$(HOST_EXE): tools/test_graphics_service.c kernel/src/arch/x86_64/syscall.c $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -I kernel/include -I build/limine-protocol/include $< -Wl,--gc-sections -o $@
test-core: test-graphics-service

.PHONY: test-time-contracts
test-core: test-time-contracts
test-time-contracts:
	CC="$(HOST_CC)" $(PYTHON) tools/test_time_contracts.py

.PHONY: test-sync-contracts
test-core: test-sync-contracts
test-sync-contracts:
	CC="$(HOST_CC)" $(PYTHON) tools/test_sync_contracts.py

.PHONY: test-desktop-runtime
test-desktop: test-desktop-runtime
test-desktop-runtime: build/tests/desktop-runtime-host$(HOST_EXE)
	$<
build/tests/desktop-runtime-host$(HOST_EXE): tools/test_desktop_runtime.c userspace/desktop_runtime/runtime.c userspace/desktop_runtime/runtime_jobs.c userspace/desktop_runtime/commands.c userspace/lib/include/srvros/desk_runtime.h userspace/lib/include/srvros/desk_commands.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include $(filter %.c,$^) -o $@

.PHONY: test-tool-commands
test-desktop: test-tool-commands
test-tool-commands: build/tests/tool-commands-host$(HOST_EXE)
	$<
build/tests/tool-commands-host$(HOST_EXE): tools/test_tool_commands.c userspace/desktools/menus.c userspace/desktools/app.h userspace/desktop_runtime/commands.c userspace/lib/include/srvros/desk_commands.h $(HOST_DESK_WIDGETS_A) $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_tool_commands.c userspace/desktop_runtime/commands.c $(HOST_DESK_WIDGETS_A) userspace/lib/src/desk_render.c userspace/lib/src/desk_font.c -Wl,--gc-sections -o $@

.PHONY: test-futex-contracts
test-core: test-futex-contracts
test-futex-contracts:
	CC="$(HOST_CC)" $(PYTHON) tools/test_futex_contracts.py

.PHONY: test-thread-smoke-harness
test-core: test-thread-smoke-harness
test-thread-smoke-harness:
	$(PYTHON) tools/test_thread_smoke.py

.PHONY: test-event-adapters
test-core: test-event-adapters
test-event-adapters:
	CC="$(HOST_CC)" $(PYTHON) tools/test_event_adapters.py

.PHONY: test-exfat-import
test-core: test-exfat-import
test-exfat-import:
	$(PYTHON) tools/test_exfat_generator.py
	CC="$(HOST_CC)" $(PYTHON) tools/test_exfat_import.py

.PHONY: test-window-set
test-desktop: test-window-set
test-window-set: build/tests/window-set-host$(HOST_EXE)
	$<
build/tests/window-set-host$(HOST_EXE): tools/test_window_set.c userspace/desktop_runtime/window_set.c userspace/desktop_runtime/runtime.c userspace/lib/include/srvros/desk_window_set.h userspace/lib/include/srvros/desk_runtime.h
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -idirafter userspace/lib/include -I shared/include tools/test_window_set.c userspace/desktop_runtime/runtime.c -o $@

.PHONY: test-terminal-commands
test-desktop: test-terminal-commands
test-terminal-commands: build/tests/terminal-commands-host$(HOST_EXE)
	$<
build/tests/terminal-commands-host$(HOST_EXE): tools/test_terminal_commands.c userspace/deskterm/deskterm.c userspace/desktop_runtime/commands.c userspace/lib/include/srvros/desk_commands.h $(DESK_WIDGETS_HEADERS)
	@mkdir -p $(@D)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -idirafter userspace/lib/include -I shared/include -I $(DESK_WIDGETS_INCLUDE) tools/test_terminal_commands.c userspace/desktop_runtime/commands.c -Wl,--gc-sections -o $@

.PHONY: test-storage-leases
test-storage-leases: build/tests/block-host$(HOST_EXE) build/tests/workspace-host$(HOST_EXE) build/tests/usb-storage-host$(HOST_EXE) build/tests/workspace-image-host$(HOST_EXE)
	build/tests/block-host$(HOST_EXE)
	build/tests/workspace-host$(HOST_EXE)
	build/tests/usb-storage-host$(HOST_EXE)
	$(PYTHON) tools/test_storage_leases.py
