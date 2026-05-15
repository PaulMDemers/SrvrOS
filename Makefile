.SUFFIXES:

ARCH := x86_64
KERNEL := build/kernel/srvros.elf
IMAGE := build/srvros-$(ARCH).iso
ISO_ROOT := build/iso_root
INITRAMFS_ROOT := build/initramfs_root
INITRAMFS := build/initramfs.tar
USER_INIT := build/userspace/init.elf
USER_HELLO := build/userspace/hello.elf
USER_CAT := build/userspace/cat.elf
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
USER_SED := build/userspace/sed.elf
USER_EXPR := build/userspace/expr.elf
USER_PRINTF := build/userspace/printf.elf
USER_TR := build/userspace/tr.elf
USER_STAT := build/userspace/stat.elf
USER_CHMOD := build/userspace/chmod.elf
USER_CP := build/userspace/cp.elf
USER_RM := build/userspace/rm.elf
USER_MKDIR := build/userspace/mkdir.elf
USER_MV := build/userspace/mv.elf
USER_TAP := build/userspace/tap.elf
USER_WEBD := build/userspace/webd.elf
USER_SPIN := build/userspace/spin.elf
USER_FPDEMO := build/userspace/fpdemo.elf
USER_UI := build/userspace/ui.elf
USER_DESKTOP := build/userspace/desktop.elf
USER_CALCGUI := build/userspace/calcgui.elf
USER_NOTESGUI := build/userspace/notesgui.elf
USER_TEXTEDIT := build/userspace/textedit.elf
USER_IMGEDIT := build/userspace/imgedit.elf
USER_POSIXDEMO := build/userspace/posixdemo.elf
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
EXFAT_IMAGE := build/srvros.exfat
SECOND_EXFAT_IMAGE := build/srvros-secondary.exfat

LIMINE_DIR := build/limine-binary
LIMINE_PROTOCOL_DIR := build/limine-protocol
LIMINE_TOOL := $(LIMINE_DIR)/limine.exe

ZIG_VERSION := 0.15.2
ZIG_DIR := build/tooling/zig
ZIG := $(ZIG_DIR)/zig.exe
CC := $(ZIG) cc
LD := $(ZIG) ld.lld
QEMU := qemu-system-x86_64

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

KERNEL_C := $(shell find kernel/src -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
KERNEL_S := $(shell find kernel/src -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
KERNEL_OBJ := $(KERNEL_C:%.c=build/%.c.o) $(KERNEL_S:%.S=build/%.S.o)
KERNEL_DEP := $(KERNEL_C:%.c=build/%.c.d) $(KERNEL_S:%.S=build/%.S.d)

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

USER_WEBD_C := $(shell find userspace/webd -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_WEBD_S := $(shell find userspace/webd -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_WEBD_OBJ := $(USER_WEBD_C:%.c=build/%.c.o) $(USER_WEBD_S:%.S=build/%.S.o)
USER_WEBD_DEP := $(USER_WEBD_C:%.c=build/%.c.d) $(USER_WEBD_S:%.S=build/%.S.d)

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

USER_CALCGUI_C := $(shell find userspace/calcgui -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_CALCGUI_S := $(shell find userspace/calcgui -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_CALCGUI_OBJ := $(USER_CALCGUI_C:%.c=build/%.c.o) $(USER_CALCGUI_S:%.S=build/%.S.o)
USER_CALCGUI_DEP := $(USER_CALCGUI_C:%.c=build/%.c.d) $(USER_CALCGUI_S:%.S=build/%.S.d)

USER_NOTESGUI_C := $(shell find userspace/notesgui -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_NOTESGUI_S := $(shell find userspace/notesgui -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_NOTESGUI_OBJ := $(USER_NOTESGUI_C:%.c=build/%.c.o) $(USER_NOTESGUI_S:%.S=build/%.S.o)
USER_NOTESGUI_DEP := $(USER_NOTESGUI_C:%.c=build/%.c.d) $(USER_NOTESGUI_S:%.S=build/%.S.d)

USER_TEXTEDIT_C := $(shell find userspace/textedit -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_TEXTEDIT_S := $(shell find userspace/textedit -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_TEXTEDIT_OBJ := $(USER_TEXTEDIT_C:%.c=build/%.c.o) $(USER_TEXTEDIT_S:%.S=build/%.S.o)
USER_TEXTEDIT_DEP := $(USER_TEXTEDIT_C:%.c=build/%.c.d) $(USER_TEXTEDIT_S:%.S=build/%.S.d)

USER_IMGEDIT_C := $(shell find userspace/imgedit -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_IMGEDIT_S := $(shell find userspace/imgedit -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_IMGEDIT_OBJ := $(USER_IMGEDIT_C:%.c=build/%.c.o) $(USER_IMGEDIT_S:%.S=build/%.S.o)
USER_IMGEDIT_DEP := $(USER_IMGEDIT_C:%.c=build/%.c.d) $(USER_IMGEDIT_S:%.S=build/%.S.d)

USER_POSIXDEMO_C := $(shell find userspace/posixdemo -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_POSIXDEMO_S := $(shell find userspace/posixdemo -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_POSIXDEMO_OBJ := $(USER_POSIXDEMO_C:%.c=build/%.c.o) $(USER_POSIXDEMO_S:%.S=build/%.S.o)
USER_POSIXDEMO_DEP := $(USER_POSIXDEMO_C:%.c=build/%.c.d) $(USER_POSIXDEMO_S:%.S=build/%.S.d)

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

USER_LIB_C := $(shell find userspace/lib/src -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_LIB_S := $(shell find userspace/lib/src -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_LIB_OBJ := $(USER_LIB_C:%.c=build/%.c.o) $(USER_LIB_S:%.S=build/%.S.o)
USER_LIB_DEP := $(USER_LIB_C:%.c=build/%.c.d) $(USER_LIB_S:%.S=build/%.S.d)

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

CJSON_C := ports/upstream/cjson/cJSON.c
CJSON_OBJ := $(CJSON_C:%.c=build/%.c.o)
CJSON_DEP := $(CJSON_C:%.c=build/%.c.d)

INI_C := ports/upstream/inih/ini.c
INI_OBJ := $(INI_C:%.c=build/%.c.o)
INI_DEP := $(INI_C:%.c=build/%.c.d)

LINENOISE_C := ports/srvros/linenoise.c
LINENOISE_OBJ := $(LINENOISE_C:%.c=build/%.c.o)
LINENOISE_DEP := $(LINENOISE_C:%.c=build/%.c.d)

SQLITE_C := ports/upstream/sqlite/sqlite3.c
SQLITE_OBJ := $(SQLITE_C:%.c=build/%.c.o)
SQLITE_DEP := $(SQLITE_C:%.c=build/%.c.d)

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

$(LIMINE_DIR)/.ready:
	mkdir -p build
	rm -rf $(LIMINE_DIR)
	git clone --depth=1 --branch=v11.x-binary https://github.com/limine-bootloader/limine.git $(LIMINE_DIR)
	touch $@

$(LIMINE_PROTOCOL_DIR)/.ready:
	mkdir -p build
	rm -rf $(LIMINE_PROTOCOL_DIR)
	git clone --depth=1 https://github.com/limine-bootloader/limine-protocol.git $(LIMINE_PROTOCOL_DIR)
	touch $@

$(ZIG):
	mkdir -p build/tooling
	curl -L https://ziglang.org/download/$(ZIG_VERSION)/zig-x86_64-windows-$(ZIG_VERSION).zip -o build/tooling/zig.zip
	rm -rf build/tooling/zig-x86_64-windows-$(ZIG_VERSION) $(ZIG_DIR)
	unzip -q build/tooling/zig.zip -d build/tooling
	mv build/tooling/zig-x86_64-windows-$(ZIG_VERSION) $(ZIG_DIR)

$(KERNEL): $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready $(KERNEL_OBJ) kernel/linker.ld
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(KERNEL_OBJ) -o $@

build/kernel/src/arch/x86_64/fpu.c.o: kernel/src/arch/x86_64/fpu.c $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -mno-sse -mno-sse2 -c $< -o $@

build/%.c.o: %.c $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.S.o: %.S $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(USER_INIT): $(ZIG) $(USER_CRT0_OBJ) $(USER_INIT_OBJ) $(USER_LIB_OBJ) userspace/linker.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_LDFLAGS) $(USER_CRT0_OBJ) $(USER_INIT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_HELLO): $(ZIG) $(USER_CRT0_OBJ) $(USER_HELLO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_HELLO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CAT): $(ZIG) $(USER_CRT0_OBJ) $(USER_CAT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CAT_OBJ) $(USER_LIB_OBJ) -o $@

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

$(USER_WEBD): $(ZIG) $(USER_CRT0_OBJ) $(USER_WEBD_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_WEBD_OBJ) $(USER_LIB_OBJ) -o $@

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

$(USER_CALCGUI): $(ZIG) $(USER_CRT0_OBJ) $(USER_CALCGUI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_CALCGUI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_NOTESGUI): $(ZIG) $(USER_CRT0_OBJ) $(USER_NOTESGUI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_NOTESGUI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TEXTEDIT): $(ZIG) $(USER_CRT0_OBJ) $(USER_TEXTEDIT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_TEXTEDIT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_IMGEDIT): $(ZIG) $(USER_CRT0_OBJ) $(USER_IMGEDIT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_IMGEDIT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_POSIXDEMO): $(ZIG) $(USER_CRT0_OBJ) $(USER_POSIXDEMO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_POSIXDEMO_OBJ) $(USER_LIB_OBJ) -o $@

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

$(USER_LUA): $(ZIG) $(USER_CRT0_OBJ) $(USER_LUA_OBJ) $(LUA_CORE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(USER_LUA_OBJ) $(LUA_CORE_OBJ) $(USER_LIB_OBJ) -o $@

build/userspace/%.c.o: userspace/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/userspace/lua/%.c.o: userspace/lua/%.c $(ZIG) $(LUA_PREPARED)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I $(LUA_SRVROS_DIR) -c $< -o $@

build/userspace/zlibdemo/%.c.o: userspace/zlibdemo/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/zlib -c $< -o $@

build/userspace/jsondemo/%.c.o: userspace/jsondemo/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/cjson -c $< -o $@

build/userspace/inidemo/%.c.o: userspace/inidemo/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/inih -c $< -o $@

build/userspace/linedemo/%.c.o: userspace/linedemo/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/linenoise -c $< -o $@

build/userspace/sqlitedemo/%.c.o: userspace/sqlitedemo/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/sqlite -c $< -o $@

build/userspace/sh/%.c.o: userspace/sh/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/linenoise -c $< -o $@

build/userspace/%.S.o: userspace/%.S $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/ports/upstream/zlib/%.c.o: ports/upstream/zlib/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/zlib -Wno-error -c $< -o $@

build/ports/upstream/cjson/%.c.o: ports/upstream/cjson/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/cjson -Wno-error -c $< -o $@

build/ports/upstream/inih/%.c.o: ports/upstream/inih/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/inih -Wno-error -c $< -o $@

build/ports/srvros/%.c.o: ports/srvros/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/linenoise -c $< -o $@

build/ports/upstream/sqlite/%.c.o: ports/upstream/sqlite/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I ports/upstream/sqlite -DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_OMIT_LOCALTIME -Wno-error -Wno-unused-parameter -Wno-sign-compare -c $< -o $@

$(LUA_PREPARED): tools/prepare_lua_port.py $(shell find ports/upstream/lua -maxdepth 1 -type f 2>/dev/null | LC_ALL=C sort)
	mkdir -p $(LUA_SRVROS_DIR)
	python3 tools/prepare_lua_port.py ports/upstream/lua $(LUA_SRVROS_DIR)

$(LUA_CORE_C): $(LUA_PREPARED)
	@true

$(LUA_SRVROS_DIR)/%.c.o: $(LUA_SRVROS_DIR)/%.c $(ZIG) $(LUA_PREPARED)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I $(LUA_SRVROS_DIR) -DNDEBUG -Dl_signalT=int -Wno-error -Wno-unused-parameter -Wno-unused-function -Wno-missing-braces -c $< -o $@

$(EXFAT_IMAGE): tools/mk_exfat_image.py $(USER_HELLO) $(USER_CAT) $(USER_SH) $(USER_LS) $(USER_ECHO) $(USER_WRITE) $(USER_WC) $(USER_CLEAR) $(USER_PS) $(USER_KILL) $(USER_WHICH) $(USER_ENV) $(USER_PWD) $(USER_TRUE) $(USER_FALSE) $(USER_SLEEP) $(USER_DATE) $(USER_TOUCH) $(USER_MKTEMP) $(USER_BASENAME) $(USER_DIRNAME) $(USER_GREP) $(USER_HEAD) $(USER_TAIL) $(USER_TEE) $(USER_UNAME) $(USER_HOSTNAME) $(USER_UPTIME) $(USER_FIND) $(USER_DU) $(USER_DF) $(USER_SORT) $(USER_UNIQ) $(USER_CUT) $(USER_XARGS) $(USER_SED) $(USER_EXPR) $(USER_PRINTF) $(USER_TR) $(USER_STAT) $(USER_CHMOD) $(USER_CP) $(USER_RM) $(USER_MKDIR) $(USER_MV) $(USER_TAP) $(USER_WEBD) $(USER_SPIN) $(USER_FPDEMO) $(USER_UI) $(USER_DESKTOP) $(USER_CALCGUI) $(USER_NOTESGUI) $(USER_TEXTEDIT) $(USER_IMGEDIT) $(USER_POSIXDEMO) $(USER_EXECDEMO) $(USER_FDPROBE) $(USER_LOCKPROBE) $(USER_TTYDEMO) $(USER_JSONDEMO) $(USER_INIDEMO) $(USER_LINEDEMO) $(USER_SQLITEDEMO) $(USER_ZLIBDEMO) $(USER_LUA)
	mkdir -p $(dir $@)
	python3 tools/mk_exfat_image.py $@ hello=$(USER_HELLO) cat=$(USER_CAT) sh=$(USER_SH) ls=$(USER_LS) echo=$(USER_ECHO) write=$(USER_WRITE) wc=$(USER_WC) clear=$(USER_CLEAR) ps=$(USER_PS) kill=$(USER_KILL) which=$(USER_WHICH) env=$(USER_ENV) pwd=$(USER_PWD) true=$(USER_TRUE) false=$(USER_FALSE) sleep=$(USER_SLEEP) date=$(USER_DATE) touch=$(USER_TOUCH) mktemp=$(USER_MKTEMP) basename=$(USER_BASENAME) dirname=$(USER_DIRNAME) grep=$(USER_GREP) head=$(USER_HEAD) tail=$(USER_TAIL) tee=$(USER_TEE) uname=$(USER_UNAME) hostname=$(USER_HOSTNAME) uptime=$(USER_UPTIME) find=$(USER_FIND) du=$(USER_DU) df=$(USER_DF) sort=$(USER_SORT) uniq=$(USER_UNIQ) cut=$(USER_CUT) xargs=$(USER_XARGS) sed=$(USER_SED) expr=$(USER_EXPR) printf=$(USER_PRINTF) tr=$(USER_TR) stat=$(USER_STAT) chmod=$(USER_CHMOD) cp=$(USER_CP) rm=$(USER_RM) mkdir=$(USER_MKDIR) mv=$(USER_MV) tap=$(USER_TAP) webd=$(USER_WEBD) spin=$(USER_SPIN) fpdemo=$(USER_FPDEMO) ui=$(USER_UI) desktop=$(USER_DESKTOP) calcgui=$(USER_CALCGUI) notesgui=$(USER_NOTESGUI) textedit=$(USER_TEXTEDIT) imgedit=$(USER_IMGEDIT) posixdemo=$(USER_POSIXDEMO) execdemo=$(USER_EXECDEMO) fdprobe=$(USER_FDPROBE) lockprobe=$(USER_LOCKPROBE) ttydemo=$(USER_TTYDEMO) jsondemo=$(USER_JSONDEMO) inidemo=$(USER_INIDEMO) linedemo=$(USER_LINEDEMO) sqlitedemo=$(USER_SQLITEDEMO) zlibdemo=$(USER_ZLIBDEMO) lua=$(USER_LUA)

$(SECOND_EXFAT_IMAGE): $(EXFAT_IMAGE)
	cp $(EXFAT_IMAGE) $(SECOND_EXFAT_IMAGE)

$(INITRAMFS): $(USER_INIT) $(USER_SH) $(USER_LS) $(USER_ECHO) $(USER_WRITE) $(USER_WC) $(USER_CLEAR) $(USER_PS) $(USER_KILL) $(USER_WHICH) $(USER_ENV) $(USER_PWD) $(USER_TRUE) $(USER_FALSE) $(USER_SLEEP) $(USER_DATE) $(USER_TOUCH) $(USER_MKTEMP) $(USER_BASENAME) $(USER_DIRNAME) $(USER_GREP) $(USER_HEAD) $(USER_TAIL) $(USER_TEE) $(USER_UNAME) $(USER_HOSTNAME) $(USER_UPTIME) $(USER_FIND) $(USER_DU) $(USER_DF) $(USER_SORT) $(USER_UNIQ) $(USER_CUT) $(USER_XARGS) $(USER_SED) $(USER_EXPR) $(USER_PRINTF) $(USER_TR) $(USER_STAT) $(USER_CHMOD) $(USER_CP) $(USER_RM) $(USER_MKDIR) $(USER_MV) $(USER_TAP) $(USER_WEBD) $(USER_SPIN) $(USER_FPDEMO) $(USER_UI) $(USER_DESKTOP) $(USER_CALCGUI) $(USER_NOTESGUI) $(USER_TEXTEDIT) $(USER_IMGEDIT) $(USER_POSIXDEMO) $(USER_EXECDEMO) $(USER_FDPROBE) $(USER_LOCKPROBE) $(USER_TTYDEMO) $(USER_JSONDEMO) $(USER_INIDEMO) $(USER_LINEDEMO) $(USER_SQLITEDEMO) $(USER_ZLIBDEMO) $(USER_LUA) $(EXFAT_IMAGE) $(shell find initramfs -type f 2>/dev/null | LC_ALL=C sort)
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
	cp $(USER_SED) $(INITRAMFS_ROOT)/sed
	cp $(USER_EXPR) $(INITRAMFS_ROOT)/expr
	cp $(USER_PRINTF) $(INITRAMFS_ROOT)/printf
	cp $(USER_TR) $(INITRAMFS_ROOT)/tr
	cp $(USER_STAT) $(INITRAMFS_ROOT)/stat
	cp $(USER_CHMOD) $(INITRAMFS_ROOT)/chmod
	cp $(USER_CP) $(INITRAMFS_ROOT)/cp
	cp $(USER_RM) $(INITRAMFS_ROOT)/rm
	cp $(USER_MKDIR) $(INITRAMFS_ROOT)/mkdir
	cp $(USER_MV) $(INITRAMFS_ROOT)/mv
	cp $(USER_TAP) $(INITRAMFS_ROOT)/tap
	cp $(USER_WEBD) $(INITRAMFS_ROOT)/webd
	cp $(USER_SPIN) $(INITRAMFS_ROOT)/spin
	cp $(USER_FPDEMO) $(INITRAMFS_ROOT)/fpdemo
	cp $(USER_UI) $(INITRAMFS_ROOT)/ui
	cp $(USER_DESKTOP) $(INITRAMFS_ROOT)/desktop
	cp $(USER_CALCGUI) $(INITRAMFS_ROOT)/calcgui
	cp $(USER_NOTESGUI) $(INITRAMFS_ROOT)/notesgui
	cp $(USER_TEXTEDIT) $(INITRAMFS_ROOT)/textedit
	cp $(USER_IMGEDIT) $(INITRAMFS_ROOT)/imgedit
	cp $(USER_POSIXDEMO) $(INITRAMFS_ROOT)/posixdemo
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
	cp $(EXFAT_IMAGE) $(INITRAMFS_ROOT)/srvros.exfat
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
	rm -rf build/kernel build/userspace build/iso_root $(INITRAMFS_ROOT) $(IMAGE) $(INITRAMFS) $(EXFAT_IMAGE) $(SECOND_EXFAT_IMAGE)

.PHONY: distclean
distclean:
	rm -rf build

-include $(KERNEL_DEP) $(USER_INIT_DEP) $(USER_HELLO_DEP) $(USER_CAT_DEP) $(USER_SH_DEP) $(USER_LS_DEP) $(USER_ECHO_DEP) $(USER_WRITE_DEP) $(USER_WC_DEP) $(USER_CLEAR_DEP) $(USER_PS_DEP) $(USER_KILL_DEP) $(USER_WHICH_DEP) $(USER_ENV_DEP) $(USER_PWD_DEP) $(USER_TRUE_DEP) $(USER_FALSE_DEP) $(USER_SLEEP_DEP) $(USER_DATE_DEP) $(USER_TOUCH_DEP) $(USER_MKTEMP_DEP) $(USER_BASENAME_DEP) $(USER_DIRNAME_DEP) $(USER_GREP_DEP) $(USER_HEAD_DEP) $(USER_TAIL_DEP) $(USER_TEE_DEP) $(USER_UNAME_DEP) $(USER_HOSTNAME_DEP) $(USER_UPTIME_DEP) $(USER_FIND_DEP) $(USER_DU_DEP) $(USER_DF_DEP) $(USER_SORT_DEP) $(USER_UNIQ_DEP) $(USER_CUT_DEP) $(USER_XARGS_DEP) $(USER_SED_DEP) $(USER_EXPR_DEP) $(USER_PRINTF_DEP) $(USER_TR_DEP) $(USER_STAT_DEP) $(USER_CHMOD_DEP) $(USER_CP_DEP) $(USER_RM_DEP) $(USER_MKDIR_DEP) $(USER_MV_DEP) $(USER_TAP_DEP) $(USER_WEBD_DEP) $(USER_SPIN_DEP) $(USER_FPDEMO_DEP) $(USER_UI_DEP) $(USER_DESKTOP_DEP) $(USER_CALCGUI_DEP) $(USER_NOTESGUI_DEP) $(USER_TEXTEDIT_DEP) $(USER_IMGEDIT_DEP) $(USER_POSIXDEMO_DEP) $(USER_EXECDEMO_DEP) $(USER_FDPROBE_DEP) $(USER_LOCKPROBE_DEP) $(USER_TTYDEMO_DEP) $(USER_JSONDEMO_DEP) $(USER_INIDEMO_DEP) $(USER_LINEDEMO_DEP) $(USER_SQLITEDEMO_DEP) $(USER_ZLIBDEMO_DEP) $(USER_LUA_DEP) $(USER_LIB_DEP) $(ZLIB_DEP) $(CJSON_DEP) $(INI_DEP) $(LINENOISE_DEP) $(SQLITE_DEP) $(LUA_CORE_DEP)
