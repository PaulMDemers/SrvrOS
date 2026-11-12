/* Included by main and its host fixture. Only the isolated held-reset/interval images
 * may run its exact terminal capture before user address spaces are created.
 * It still uses all ordinary kernel services and admission checks. */
static bool boot_system_init(const char *cmdline, bool timer_running) {
#ifdef SRVROS_INTEL_ENTRY_REQUESTER
    if(intel_gt_boot_entry_requester_recipe(cmdline)) {
        console_write("init: deferred=entry-requester-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_RING_RESET)
    if(intel_gt_boot_ring_reset_recipe(cmdline)) {
        console_write("init: deferred=ring-reset-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_RING_ENTRY)
    if (intel_gt_boot_ring_entry_recipe(cmdline)) {
        console_write("init: deferred=ring-entry-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_RING_TRANSITION)
    if (intel_gt_boot_ring_transition_recipe(cmdline)) {
        console_write("init: deferred=ring-transition-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_HDA_SHUTDOWN)
    if (intel_gt_boot_hda_shutdown_recipe(cmdline)) {
        console_write("init: deferred=hda-bcs-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_GPU_HOLD)
    if (intel_gt_boot_gpu_hold_recipe(cmdline)) {
        console_write("init: deferred=gpu-hold-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_JOINT_ADMIT)
    if (intel_gt_boot_joint_admit_recipe(cmdline)) {
        console_write("init: deferred=joint-admit-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_JOINT)
    if (intel_gt_boot_joint_recipe(cmdline)) {
        console_write("init: deferred=joint-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_REHOLD)
    if (intel_gt_boot_rehold_recipe(cmdline)) {
        console_write("init: deferred=rehold-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_HELD_INTERVAL)
    if (intel_gt_boot_held_interval_recipe(cmdline)) {
        console_write("init: deferred=held-interval-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_PREPARE_RESET)
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
