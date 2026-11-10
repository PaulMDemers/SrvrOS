/* Included by main and its host fixture. Only the isolated held-reset image
 * may run its exact terminal capture before user address spaces are created.
 * It still uses all ordinary kernel services and admission checks. */
static bool boot_system_init(const char *cmdline, bool timer_running) {
#ifdef SRVROS_INTEL_PREPARE_RESET
    if (intel_gt_boot_held_reset_recipe(cmdline)) {
        console_write("init: deferred=held-reset-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#else
    (void)cmdline;
#endif
    bool init_ready = false;
    int64_t init_pid = process_spawn_background_elf_args_quiet("/init", "--system");
    if (init_pid >= 0) {
        (void)process_set_reaper((uint64_t)init_pid);
        console_printf("init: started pid=%u\n", (uint64_t)init_pid);
        uint64_t init_start = timer_monotonic_us();
        bool ready = timer_running && process_wait_startup((uint64_t)init_pid, 300);
        init_ready = ready;
        console_printf("init: readiness=%s elapsed_us=%u\n", ready ? "ready" : (timer_running ? "timeout-or-exit" : "clock-unavailable"),
            timer_monotonic_us() - init_start);
    } else {
        console_write("init: start failed\n");
    }
    return init_ready;
}
