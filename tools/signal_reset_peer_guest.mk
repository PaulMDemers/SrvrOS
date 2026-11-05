# Matched private-test kernel only; no normal syscall or public ABI.
build/signal-context/reset-peer-review/guest.c.o: tools/test_signal_reset_peer_guest.c kernel/src/process_signal.h $(USER_SYSROOT_HEADERS)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/signal-context/reset-peer-review/guest.S.o: tools/test_signal_reset_peer_guest.S
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/signal-context/reset-peer-review/guest.elf: build/signal-context/reset-peer-review/guest.c.o build/signal-context/reset-peer-review/guest.S.o $(USER_LIB_OBJ) $(USER_CRT0_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) build/signal-context/reset-peer-review/guest.c.o build/signal-context/reset-peer-review/guest.S.o $(USER_LIB_OBJ) -o $@
