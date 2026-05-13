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
USER_GREP := build/userspace/grep.elf
USER_HEAD := build/userspace/head.elf
USER_STAT := build/userspace/stat.elf
USER_CP := build/userspace/cp.elf
USER_RM := build/userspace/rm.elf
USER_MKDIR := build/userspace/mkdir.elf
USER_MV := build/userspace/mv.elf
USER_WEBD := build/userspace/webd.elf
USER_SPIN := build/userspace/spin.elf
USER_UI := build/userspace/ui.elf
USER_DESKTOP := build/userspace/desktop.elf
USER_CALCGUI := build/userspace/calcgui.elf
USER_NOTESGUI := build/userspace/notesgui.elf
USER_TEXTEDIT := build/userspace/textedit.elf
USER_IMGEDIT := build/userspace/imgedit.elf
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
	-mno-sse \
	-mno-sse2 \
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
	-mno-sse \
	-mno-sse2 \
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

USER_GREP_C := $(shell find userspace/grep -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_GREP_S := $(shell find userspace/grep -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_GREP_OBJ := $(USER_GREP_C:%.c=build/%.c.o) $(USER_GREP_S:%.S=build/%.S.o)
USER_GREP_DEP := $(USER_GREP_C:%.c=build/%.c.d) $(USER_GREP_S:%.S=build/%.S.d)

USER_HEAD_C := $(shell find userspace/head -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_HEAD_S := $(shell find userspace/head -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_HEAD_OBJ := $(USER_HEAD_C:%.c=build/%.c.o) $(USER_HEAD_S:%.S=build/%.S.o)
USER_HEAD_DEP := $(USER_HEAD_C:%.c=build/%.c.d) $(USER_HEAD_S:%.S=build/%.S.d)

USER_STAT_C := $(shell find userspace/stat -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_STAT_S := $(shell find userspace/stat -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_STAT_OBJ := $(USER_STAT_C:%.c=build/%.c.o) $(USER_STAT_S:%.S=build/%.S.o)
USER_STAT_DEP := $(USER_STAT_C:%.c=build/%.c.d) $(USER_STAT_S:%.S=build/%.S.d)

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

USER_WEBD_C := $(shell find userspace/webd -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_WEBD_S := $(shell find userspace/webd -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_WEBD_OBJ := $(USER_WEBD_C:%.c=build/%.c.o) $(USER_WEBD_S:%.S=build/%.S.o)
USER_WEBD_DEP := $(USER_WEBD_C:%.c=build/%.c.d) $(USER_WEBD_S:%.S=build/%.S.d)

USER_SPIN_C := $(shell find userspace/spin -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_SPIN_S := $(shell find userspace/spin -type f -name '*.S' 2>/dev/null | LC_ALL=C sort)
USER_SPIN_OBJ := $(USER_SPIN_C:%.c=build/%.c.o) $(USER_SPIN_S:%.S=build/%.S.o)
USER_SPIN_DEP := $(USER_SPIN_C:%.c=build/%.c.d) $(USER_SPIN_S:%.S=build/%.S.d)

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

USER_LIB_C := $(shell find userspace/lib/src -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
USER_LIB_OBJ := $(USER_LIB_C:%.c=build/%.c.o)
USER_LIB_DEP := $(USER_LIB_C:%.c=build/%.c.d)

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

build/%.c.o: %.c $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.S.o: %.S $(ZIG) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(USER_INIT): $(ZIG) $(USER_INIT_OBJ) $(USER_LIB_OBJ) userspace/linker.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_LDFLAGS) $(USER_INIT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_HELLO): $(ZIG) $(USER_HELLO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_HELLO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CAT): $(ZIG) $(USER_CAT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CAT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SH): $(ZIG) $(USER_SH_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_SH_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_LS): $(ZIG) $(USER_LS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_LS_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_ECHO): $(ZIG) $(USER_ECHO_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_ECHO_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_WRITE): $(ZIG) $(USER_WRITE_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_WRITE_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_WC): $(ZIG) $(USER_WC_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_WC_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CLEAR): $(ZIG) $(USER_CLEAR_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CLEAR_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_PS): $(ZIG) $(USER_PS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_PS_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_KILL): $(ZIG) $(USER_KILL_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_KILL_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_GREP): $(ZIG) $(USER_GREP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_GREP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_HEAD): $(ZIG) $(USER_HEAD_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_HEAD_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_STAT): $(ZIG) $(USER_STAT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_STAT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CP): $(ZIG) $(USER_CP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_RM): $(ZIG) $(USER_RM_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_RM_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_MKDIR): $(ZIG) $(USER_MKDIR_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_MKDIR_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_MV): $(ZIG) $(USER_MV_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_MV_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_WEBD): $(ZIG) $(USER_WEBD_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_WEBD_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_SPIN): $(ZIG) $(USER_SPIN_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_SPIN_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_UI): $(ZIG) $(USER_UI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_UI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_DESKTOP): $(ZIG) $(USER_DESKTOP_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_DESKTOP_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_CALCGUI): $(ZIG) $(USER_CALCGUI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CALCGUI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_NOTESGUI): $(ZIG) $(USER_NOTESGUI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_NOTESGUI_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_TEXTEDIT): $(ZIG) $(USER_TEXTEDIT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_TEXTEDIT_OBJ) $(USER_LIB_OBJ) -o $@

$(USER_IMGEDIT): $(ZIG) $(USER_IMGEDIT_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_IMGEDIT_OBJ) $(USER_LIB_OBJ) -o $@

build/userspace/%.c.o: userspace/%.c $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/userspace/%.S.o: userspace/%.S $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(EXFAT_IMAGE): tools/mk_exfat_image.py $(USER_HELLO) $(USER_CAT) $(USER_SH) $(USER_LS) $(USER_ECHO) $(USER_WRITE) $(USER_WC) $(USER_CLEAR) $(USER_PS) $(USER_KILL) $(USER_GREP) $(USER_HEAD) $(USER_STAT) $(USER_CP) $(USER_RM) $(USER_MKDIR) $(USER_MV) $(USER_WEBD) $(USER_SPIN) $(USER_UI) $(USER_DESKTOP) $(USER_CALCGUI) $(USER_NOTESGUI) $(USER_TEXTEDIT) $(USER_IMGEDIT)
	mkdir -p $(dir $@)
	python3 tools/mk_exfat_image.py $@ hello=$(USER_HELLO) cat=$(USER_CAT) sh=$(USER_SH) ls=$(USER_LS) echo=$(USER_ECHO) write=$(USER_WRITE) wc=$(USER_WC) clear=$(USER_CLEAR) ps=$(USER_PS) kill=$(USER_KILL) grep=$(USER_GREP) head=$(USER_HEAD) stat=$(USER_STAT) cp=$(USER_CP) rm=$(USER_RM) mkdir=$(USER_MKDIR) mv=$(USER_MV) webd=$(USER_WEBD) spin=$(USER_SPIN) ui=$(USER_UI) desktop=$(USER_DESKTOP) calcgui=$(USER_CALCGUI) notesgui=$(USER_NOTESGUI) textedit=$(USER_TEXTEDIT) imgedit=$(USER_IMGEDIT)

$(SECOND_EXFAT_IMAGE): $(EXFAT_IMAGE)
	cp $(EXFAT_IMAGE) $(SECOND_EXFAT_IMAGE)

$(INITRAMFS): $(USER_INIT) $(USER_SH) $(USER_LS) $(USER_ECHO) $(USER_WRITE) $(USER_WC) $(USER_CLEAR) $(USER_PS) $(USER_KILL) $(USER_GREP) $(USER_HEAD) $(USER_STAT) $(USER_CP) $(USER_RM) $(USER_MKDIR) $(USER_MV) $(USER_WEBD) $(USER_SPIN) $(USER_UI) $(USER_DESKTOP) $(USER_CALCGUI) $(USER_NOTESGUI) $(USER_TEXTEDIT) $(USER_IMGEDIT) $(EXFAT_IMAGE) $(shell find initramfs -type f 2>/dev/null | LC_ALL=C sort)
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
	cp $(USER_GREP) $(INITRAMFS_ROOT)/grep
	cp $(USER_HEAD) $(INITRAMFS_ROOT)/head
	cp $(USER_STAT) $(INITRAMFS_ROOT)/stat
	cp $(USER_CP) $(INITRAMFS_ROOT)/cp
	cp $(USER_RM) $(INITRAMFS_ROOT)/rm
	cp $(USER_MKDIR) $(INITRAMFS_ROOT)/mkdir
	cp $(USER_MV) $(INITRAMFS_ROOT)/mv
	cp $(USER_WEBD) $(INITRAMFS_ROOT)/webd
	cp $(USER_SPIN) $(INITRAMFS_ROOT)/spin
	cp $(USER_UI) $(INITRAMFS_ROOT)/ui
	cp $(USER_DESKTOP) $(INITRAMFS_ROOT)/desktop
	cp $(USER_CALCGUI) $(INITRAMFS_ROOT)/calcgui
	cp $(USER_NOTESGUI) $(INITRAMFS_ROOT)/notesgui
	cp $(USER_TEXTEDIT) $(INITRAMFS_ROOT)/textedit
	cp $(USER_IMGEDIT) $(INITRAMFS_ROOT)/imgedit
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
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio

.PHONY: run-net
run-net: $(IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio \
		-netdev user,id=net0,hostfwd=tcp:127.0.0.1:8080-10.0.2.15:80 \
		-device e1000,netdev=net0

.PHONY: run-ahci-net
run-ahci-net: $(IMAGE) $(EXFAT_IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio \
		-drive if=none,id=exfat,file=$(EXFAT_IMAGE),format=raw \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=exfat,bus=ahci.0 \
		-netdev user,id=net0,hostfwd=tcp:127.0.0.1:8080-10.0.2.15:80 \
		-device e1000,netdev=net0

.PHONY: run-ahci2-net
run-ahci2-net: $(IMAGE) $(EXFAT_IMAGE) $(SECOND_EXFAT_IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio \
		-drive if=none,id=exfat0,file=$(EXFAT_IMAGE),format=raw \
		-drive if=none,id=exfat1,file=$(SECOND_EXFAT_IMAGE),format=raw \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=exfat0,bus=ahci.0 \
		-device ide-hd,drive=exfat1,bus=ahci.1 \
		-netdev user,id=net0,hostfwd=tcp:127.0.0.1:8080-10.0.2.15:80 \
		-device e1000,netdev=net0

.PHONY: run-virtio-net
run-virtio-net: $(IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio \
		-netdev user,id=net0,hostfwd=tcp:127.0.0.1:8080-10.0.2.15:80 \
		-device virtio-net-pci,netdev=net0

.PHONY: debug
debug: $(IMAGE)
	$(QEMU) -M q35 -m 512M -cdrom $(IMAGE) -boot d -serial stdio -s -S

.PHONY: clean
clean:
	rm -rf build/kernel build/userspace build/iso_root $(INITRAMFS_ROOT) $(IMAGE) $(INITRAMFS) $(EXFAT_IMAGE) $(SECOND_EXFAT_IMAGE)

.PHONY: distclean
distclean:
	rm -rf build

-include $(KERNEL_DEP) $(USER_INIT_DEP) $(USER_HELLO_DEP) $(USER_CAT_DEP) $(USER_SH_DEP) $(USER_LS_DEP) $(USER_ECHO_DEP) $(USER_WRITE_DEP) $(USER_WC_DEP) $(USER_CLEAR_DEP) $(USER_PS_DEP) $(USER_KILL_DEP) $(USER_GREP_DEP) $(USER_HEAD_DEP) $(USER_STAT_DEP) $(USER_CP_DEP) $(USER_RM_DEP) $(USER_MV_DEP) $(USER_WEBD_DEP) $(USER_SPIN_DEP) $(USER_UI_DEP) $(USER_DESKTOP_DEP) $(USER_CALCGUI_DEP) $(USER_NOTESGUI_DEP) $(USER_TEXTEDIT_DEP) $(USER_IMGEDIT_DEP) $(USER_LIB_DEP)
