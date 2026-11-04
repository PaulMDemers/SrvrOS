# Production-kernel exec regression. No alternate IDT or private signal hook.
build/signal-context/exec-pending-review/guest.c.o: tools/test_exec_pending_guest.c $(USER_SYSROOT_HEADERS)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/signal-context/exec-pending-review/guest.elf: build/signal-context/exec-pending-review/guest.c.o $(USER_LIB_OBJ) $(USER_CRT0_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) build/signal-context/exec-pending-review/guest.c.o $(USER_LIB_OBJ) -o $@
