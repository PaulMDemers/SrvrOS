# Public candidate lives only in a separately linked opt-in guest kernel.
SIGNAL_PUBLIC_DIR ?= build/signal-context/public-selftest
SIGNAL_PUBLIC_FLAGS := -mno-sse -mno-sse2 -mno-mmx -mno-80387 -fno-jump-tables -DSRVROS_SIGNAL_PUBLIC_GUEST_TEST=1
ifeq ($(SIGNAL_PUBLIC_EXPERIMENTAL),1)
SIGNAL_PUBLIC_FLAGS += -DSRVROS_SIGNAL_EXPERIMENTAL_INFO=1
endif
SIGNAL_PUBLIC_REPLACED := build/kernel/src/process.c.o build/kernel/src/console.c.o build/kernel/src/arch/x86_64/idt.c.o build/kernel/src/arch/x86_64/syscall.c.o
$(SIGNAL_PUBLIC_DIR)/console.c.o: kernel/src/console.c $(KERNEL_HEADERS) tools/signal_public_guest.mk
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SIGNAL_PUBLIC_FLAGS) -c $< -o $@
$(SIGNAL_PUBLIC_DIR)/process.c.o: kernel/src/process.c kernel/src/process_signal_public.inc kernel/src/process_signal.h $(KERNEL_HEADERS) tools/signal_public_guest.mk
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SIGNAL_PUBLIC_FLAGS) -c $< -o $@
$(SIGNAL_PUBLIC_DIR)/idt.c.o: kernel/src/arch/x86_64/idt.c kernel/src/process_signal.h $(KERNEL_HEADERS) tools/signal_public_guest.mk
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SIGNAL_PUBLIC_FLAGS) -c $< -o $@
$(SIGNAL_PUBLIC_DIR)/syscall.c.o: kernel/src/arch/x86_64/syscall.c kernel/src/arch/x86_64/signal_io_backend.inc kernel/src/process_signal.h $(KERNEL_HEADERS) tools/signal_public_guest.mk
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SIGNAL_PUBLIC_FLAGS) -c $< -o $@
$(SIGNAL_PUBLIC_DIR)/kernel.elf: $(filter-out $(SIGNAL_PUBLIC_REPLACED),$(KERNEL_OBJ)) $(addprefix $(SIGNAL_PUBLIC_DIR)/,process.c.o console.c.o idt.c.o syscall.c.o)
	$(LD) $(LDFLAGS) $^ -o $@
$(SIGNAL_PUBLIC_DIR)/probe.c.o: tools/test_signal_public_guest.c $(USER_SYSROOT_HEADERS)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
$(SIGNAL_PUBLIC_DIR)/probe.S.o: tools/test_signal_delivery_guest.S
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
$(SIGNAL_PUBLIC_DIR)/probe.elf: $(SIGNAL_PUBLIC_DIR)/probe.c.o $(SIGNAL_PUBLIC_DIR)/probe.S.o $(USER_LIB_A) $(USER_CRT0_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(SIGNAL_PUBLIC_DIR)/probe.c.o $(SIGNAL_PUBLIC_DIR)/probe.S.o $(USER_LIB_A) -o $@
-include $(SIGNAL_PUBLIC_DIR)/process.c.d $(SIGNAL_PUBLIC_DIR)/console.c.d $(SIGNAL_PUBLIC_DIR)/idt.c.d $(SIGNAL_PUBLIC_DIR)/syscall.c.d $(SIGNAL_PUBLIC_DIR)/probe.c.d
