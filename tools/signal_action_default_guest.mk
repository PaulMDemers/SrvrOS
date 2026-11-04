# Native public cooperative configuration path, no private INFO hook.
build/signal-context/action-default-review/guest.c.o: tools/test_signal_action_default_guest.c $(USER_SYSROOT_HEADERS)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/signal-context/action-default-review/guest.elf: build/signal-context/action-default-review/guest.c.o $(USER_LIB_OBJ) $(USER_CRT0_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) build/signal-context/action-default-review/guest.c.o $(USER_LIB_OBJ) -o $@
