#ifndef DESKD_PROFILE_CHOOSER_H
#define DESKD_PROFILE_CHOOSER_H
#include <srvros/profile_session.h>

enum desktop_profile_choice {
    DESKTOP_PROFILE_ERROR=-1, DESKTOP_PROFILE_LEGACY=0,
    DESKTOP_PROFILE_SELECTED=1, DESKTOP_PROFILE_CONSOLE=2
};
struct desktop_profile_request {
    const char *root, *remember_path;
    unsigned scale;
    int force;
};
struct desktop_profile_result {
    struct srv_profile_session profile;
    char portable_path[160], error[160];
    /* Registration is process-owned and survives the startup UI. The chooser
     * releases its framebuffer, presentation lease and keyboard mode. */
    int server_registered;
};
/* Ordinary startup only; explicit --profile and diagnostic modes bypass this.
 * No applications have been launched when this function is called. */
int desktop_profile_choose(const struct desktop_profile_request *request,
    struct desktop_profile_result *result);
#endif
