/* Included by main and its host fixtures. Only dedicated terminal images may
 * defer user init for their exact capture recipes. Ordinary kernel services
 * and all admission checks still apply. */
#ifdef SRVROS_INTEL_EDP_BASELINE
static bool edp_boot_literal(const char **cursor, const char *literal) {
    while (*literal) {
        if (**cursor != *literal++) return false;
        ++*cursor;
    }
    return true;
}

static bool edp_boot_hex(const char **cursor, unsigned digits) {
    for (unsigned i = 0; i < digits; ++i) {
        char ch = **cursor;
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
        ++*cursor;
    }
    return true;
}

static bool edp_boot_prefix(const char **cmdline) {
    return *cmdline && edp_boot_literal(cmdline, "srvros.storage=ram srvros.workspace-volume=") &&
        edp_boot_hex(cmdline, 8) && edp_boot_literal(cmdline, " srvros.desktop-release=") &&
        edp_boot_hex(cmdline, 16) &&
        edp_boot_literal(cmdline, " srvros.capture=intel-display srvros.intel-backing=read-only"
            " srvros.intel-cache=read-only srvros.intel-edp-baseline=on");
}

static bool edp_pci_hold_recipe(const char *cmdline) {
    return edp_boot_prefix(&cmdline) && edp_boot_literal(&cmdline, " srvros.intel-edp-cycle=on"
        " srvros.intel-edp-flr=pci-hold srvros.capture-failure=reboot") && !*cmdline;
}

static bool edp_live_ggtt_recipe(const char *cmdline) {
    return edp_boot_prefix(&cmdline) && edp_boot_literal(&cmdline, " srvros.capture-failure=reboot") && !*cmdline;
}
#endif

static bool boot_system_init(const char *cmdline, bool timer_running) {
#if defined(SRVROS_INTEL_EDP_BASELINE)
    if (edp_live_ggtt_recipe(cmdline)) {
        console_write("init: deferred=edp-live-ggtt-preinit; terminal automatic capture, no user services\n");
        return false;
    }
    if (edp_pci_hold_recipe(cmdline)) {
        console_write("init: deferred=edp-pci-hold-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_STOLEN_CAPTURE)
    if(intel_stolen_probe_recipe(cmdline)) {
        console_write("init: deferred=stolen-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_GT_BOOTSTRAP_NATIVE)
    if(intel_gt_boot_gt_bootstrap_recipe(cmdline)) {
        console_write("init: deferred=gt-bootstrap-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_VTD_DRAIN)
    if(intel_gt_boot_vtd_drain_recipe(cmdline)) {
        console_write("init: deferred=vtd-drain-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_SOURCE_AUDIT)
    if(intel_gt_boot_source_audit_recipe(cmdline)) {
        console_write("init: deferred=source-audit-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_SOURCE_ENGINES)
    if(intel_gt_boot_source_engines_recipe(cmdline)) {
        console_write("init: deferred=source-engines-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_SOURCE_REQUESTER)
    if(intel_gt_boot_source_requester_recipe(cmdline)) {
        console_write("init: deferred=source-requester-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_SOURCE_TRANSFER)
    if(intel_gt_boot_startup_transfer_recipe(cmdline)) {
        console_write("init: deferred=startup-transfer-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_STARTUP_BUFFERS)
    if(intel_gt_boot_startup_buffers_recipe(cmdline)) {
        console_write("init: deferred=startup-buffers-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_STARTUP_SOURCE)
    if(intel_gt_boot_startup_source_recipe(cmdline)) {
        console_write("init: deferred=startup-source-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_NONZERO_SOURCE)
    if(intel_gt_boot_nonzero_source_recipe(cmdline)) {
        console_write("init: deferred=nonzero-source-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_NONZERO_RESET)
    if(intel_gt_boot_nonzero_reset_recipe(cmdline)) {
        console_write("init: deferred=nonzero-reset-preinit; terminal automatic capture, no user services\n");
        return false;
    }
#elif defined(SRVROS_INTEL_ENTRY_REQUESTER)
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
