# Separate opt-in kernel; ordinary kernel/IDT objects remain untouched.
build/signal-context/signal-selftest/idt.c.o: kernel/src/arch/x86_64/idt.c kernel/src/process_signal.h kernel/src/arch/x86_64/signal_state.h kernel/src/arch/x86_64/signal_context.h kernel/src/arch/x86_64/isr_frame.h $(KERNEL_HEADERS) $(LIMINE_PROTOCOL_DIR)/.ready
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -DSRVROS_SIGNAL_CONTEXT_GUEST_TEST=1 -c $< -o $@
build/signal-context/signal-selftest/kernel.elf: $(filter-out build/kernel/src/arch/x86_64/idt.c.o,$(KERNEL_OBJ)) build/signal-context/signal-selftest/idt.c.o
	mkdir -p $(@D)
	$(LD) $(LDFLAGS) $(filter-out build/kernel/src/arch/x86_64/idt.c.o,$(KERNEL_OBJ)) build/signal-context/signal-selftest/idt.c.o -o $@
build/signal-context/signal-selftest/probe.c.o: tools/test_signal_delivery_guest.c kernel/src/process_signal.h kernel/src/arch/x86_64/signal_state.h kernel/src/arch/x86_64/signal_context.h $(USER_SYSROOT_HEADERS)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/signal-context/signal-selftest/probe.S.o: tools/test_signal_delivery_guest.S
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/signal-context/signal-selftest/probe.elf: build/signal-context/signal-selftest/probe.c.o build/signal-context/signal-selftest/probe.S.o $(USER_LIB_OBJ) $(USER_CRT0_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) build/signal-context/signal-selftest/probe.c.o build/signal-context/signal-selftest/probe.S.o $(USER_LIB_OBJ) -o $@
