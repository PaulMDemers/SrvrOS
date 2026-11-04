#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/profile_paths.h>

static void expect(const char *home, enum srv_profile_path_kind kind, const char *want) {
    char out[SRV_PATH_MAX];
    assert(srv_profile_path(out, sizeof out, home, kind));
    assert(strcmp(out, want) == 0);
}
static void invalid(const char *home) {
    char out[32] = "unchanged";
    assert(!srv_profile_home_valid(home));
    assert(!srv_profile_path(out, sizeof out, home, SRV_PROFILE_HOME));
    assert(strcmp(out, "unchanged") == 0);
}
int main(int argc, char **argv) {
    char out[SRV_PATH_MAX], longpath[SRV_PATH_MAX + 1];
    /* Used by the installation fixture to cross-check the shared C provider. */
    if (argc == 2) {
        for (int kind = SRV_PROFILE_HOME; kind <= SRV_PROFILE_CACHE; ++kind) {
            if (!srv_profile_path(out, sizeof out, argv[1], (enum srv_profile_path_kind)kind))
                return 1;
            puts(out);
        }
        return 0;
    }
    const char *bad[] = {"", "relative", "//", "/a/", "/a//b", "/./a", "/a/..",
                         "/a/../b", "/a\\b", "/a\nb", "/a\177b", "/a\200b"};
    size_t i;
    expect("/fat/home", SRV_PROFILE_HOME, "/fat/home");
    expect("/fat/home", SRV_PROFILE_CONFIG, "/fat/home/.config");
    expect("/fat/home", SRV_PROFILE_DATA, "/fat/home/.local/share");
    expect("/fat/home", SRV_PROFILE_CACHE, "/fat/home/.cache");
    expect("/work/srvros-work", SRV_PROFILE_CONFIG, "/work/srvros-work/.config");
    expect("/work/srvros-work", SRV_PROFILE_DATA, "/work/srvros-work/.local/share");
    expect("/work/srvros-work", SRV_PROFILE_CACHE, "/work/srvros-work/.cache");
    expect("/", SRV_PROFILE_HOME, "/");
    expect("/", SRV_PROFILE_DATA, "/.local/share");
    expect("/a/.../b.c", SRV_PROFILE_HOME, "/a/.../b.c");
    invalid(NULL);
    for (i = 0; i < sizeof bad / sizeof bad[0]; ++i) invalid(bad[i]);
    memset(longpath, 'a', sizeof longpath); longpath[0] = '/';
    longpath[SRV_NAME_MAX + 1] = '\0';
    assert(srv_profile_home_valid(longpath));
    longpath[SRV_NAME_MAX + 1] = 'a'; longpath[SRV_NAME_MAX + 2] = '\0';
    invalid(longpath);
    /* Four legal components reach the maximum canonical length. */
    for (i = 0; i < SRV_PATH_MAX - 1; ++i) longpath[i] = (i % 256 == 0) ? '/' : 'a';
    longpath[SRV_PATH_MAX - 1] = '\0';
    assert(srv_profile_home_valid(longpath));
    assert(srv_profile_path(out, sizeof out, longpath, SRV_PROFILE_HOME));
    strcpy(out, "unchanged");
    assert(!srv_profile_path(out, sizeof out, longpath, SRV_PROFILE_CACHE));
    assert(!strcmp(out, "unchanged"));
    longpath[SRV_PATH_MAX - 1] = 'a'; longpath[SRV_PATH_MAX] = '\0';
    invalid(longpath);
    strcpy(out, "unchanged");
    assert(!srv_profile_path(out, 0, "/a", SRV_PROFILE_HOME));
    assert(!srv_profile_path(out, 2, "/a", SRV_PROFILE_HOME));
    assert(!srv_profile_path(out, sizeof out, "/a", (enum srv_profile_path_kind)99));
    assert(!strcmp(out, "unchanged"));
    assert(!srv_profile_path(NULL, 100, "/a", SRV_PROFILE_HOME));
    assert(srv_profile_path(out, 3, "/a", SRV_PROFILE_HOME));
    strcpy(out, "/abc");
    assert(srv_profile_path(out + 1, sizeof out - 1, out, SRV_PROFILE_CONFIG));
    assert(!strcmp(out + 1, "/abc/.config"));
    puts("profile paths: ok");
    return 0;
}
