#define BOOT_SYSTEM_INIT_FIXTURE_ONLY
#include "test_boot_system_init.c"

int main(void) {
    const char *recipe = "srvros.storage=ram srvros.workspace-volume=37d8c5d0"
        " srvros.desktop-release=0123456789abcdef srvros.capture=intel-display"
        " srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-edp-baseline=on"
        " srvros.intel-edp-cycle=on srvros.intel-edp-flr=pci-hold srvros.capture-failure=reboot";
    reset();
    assert(edp_pci_hold_recipe(recipe) && !boot_system_init(recipe, true));
    assert(!spawns && !reapers && !waits && !clocks);

    char bad[512];
    for (const char *start = recipe; *start;) {
        const char *end = strchr(start, ' ');
        if (!end) end = start + strlen(start);
        snprintf(bad, sizeof bad, "%.*s%s", (int)(start - recipe), recipe, *end ? end + 1 : end);
        normal(bad);
        snprintf(bad, sizeof bad, "%s %.*s", recipe, (int)(end - start), start);
        normal(bad);
        start = *end ? end + 1 : end;
    }
    snprintf(bad, sizeof bad, "%s srvros.audio=hda", recipe);
    normal(bad);
    snprintf(bad, sizeof bad, "%s", recipe);
    strstr(bad, "37d8c5d0")[0] = 'z';
    normal(bad);
    normal(NULL);
    normal("srvros.desktop=session");
    puts("boot-edp-pci-hold: PASS exact terminal deferral and ordinary init fallbacks");
}
