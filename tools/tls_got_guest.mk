# Carry the real GOT/TLS regression with both initialized and zero TLS.
build/signal-context/tls-got-review/zero.c.o: tools/test_tls_got_guest.c $(USER_SYSROOT_HEADERS)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/signal-context/tls-got-review/initialized.c.o: tools/test_tls_got_guest.c $(USER_SYSROOT_HEADERS)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -DINITIALIZED_TLS -c $< -o $@
build/signal-context/tls-got-review/zero.elf: build/signal-context/tls-got-review/zero.c.o $(USER_LIB_OBJ) $(USER_CRT0_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) --no-relax $(USER_CRT0_OBJ) build/signal-context/tls-got-review/zero.c.o $(USER_LIB_OBJ) -o $@
build/signal-context/tls-got-review/initialized.elf: build/signal-context/tls-got-review/initialized.c.o $(USER_LIB_OBJ) $(USER_CRT0_OBJ) userspace/app.ld
	$(LD) $(USER_APP_LDFLAGS) --no-relax $(USER_CRT0_OBJ) build/signal-context/tls-got-review/initialized.c.o $(USER_LIB_OBJ) -o $@
