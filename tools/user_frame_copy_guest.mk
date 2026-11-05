# Explicit disposable test-kernel target; default kernel/object untouched.
build/signal-context/user-copy-selftest/idt.c.o: kernel/src/arch/x86_64/idt.c kernel/src/arch/x86_64/user_frame_copy.h $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -DSRVROS_USER_FRAME_COPY_GUEST_TEST=1 -c $< -o $@
build/signal-context/user-copy-selftest/kernel.elf: $(filter-out build/kernel/src/arch/x86_64/idt.c.o,$(KERNEL_OBJ)) build/signal-context/user-copy-selftest/idt.c.o
	mkdir -p $(@D)
	$(LD) $(LDFLAGS) $(filter-out build/kernel/src/arch/x86_64/idt.c.o,$(KERNEL_OBJ)) build/signal-context/user-copy-selftest/idt.c.o -o $@

build/signal-context/user-copy-selftest/probe.o: tools/test_user_frame_copy_guest.c $(USER_SYSROOT_HEADERS)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/signal-context/user-copy-selftest/probe.elf: build/signal-context/user-copy-selftest/probe.o $(USER_LIB_OBJ) $(USER_CRT0_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) build/signal-context/user-copy-selftest/probe.o $(USER_LIB_OBJ) -o $@
