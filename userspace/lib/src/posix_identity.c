#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

static struct passwd root_passwd = {
    .pw_name = "root",
    .pw_passwd = "x",
    .pw_uid = 0,
    .pw_gid = 0,
    .pw_gecos = "srvros",
    .pw_dir = "/",
    .pw_shell = "/fat/bin/sh",
};

static struct group root_group = {
    .gr_name = "root",
    .gr_passwd = "x",
    .gr_gid = 0,
    .gr_mem = 0,
};

static char *copy_string(char **cursor, size_t *remaining, const char *text) {
    size_t length = strlen(text) + 1;
    if (*remaining < length) {
        return 0;
    }
    char *out = *cursor;
    memcpy(out, text, length);
    *cursor += length;
    *remaining -= length;
    return out;
}

struct passwd *getpwnam(const char *name) {
    return name != 0 && strcmp(name, "root") == 0 ? &root_passwd : 0;
}

struct passwd *getpwuid(uid_t uid) {
    return uid == 0 ? &root_passwd : 0;
}

int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    if (result == 0 || pwd == 0 || buf == 0) {
        return EINVAL;
    }
    *result = 0;
    struct passwd *source = getpwnam(name);
    if (source == 0) {
        return 0;
    }
    char *cursor = buf;
    size_t remaining = buflen;
    *pwd = *source;
    pwd->pw_name = copy_string(&cursor, &remaining, source->pw_name);
    pwd->pw_passwd = copy_string(&cursor, &remaining, source->pw_passwd);
    pwd->pw_gecos = copy_string(&cursor, &remaining, source->pw_gecos);
    pwd->pw_dir = copy_string(&cursor, &remaining, source->pw_dir);
    pwd->pw_shell = copy_string(&cursor, &remaining, source->pw_shell);
    if (pwd->pw_name == 0 || pwd->pw_passwd == 0 || pwd->pw_gecos == 0 || pwd->pw_dir == 0 || pwd->pw_shell == 0) {
        return ERANGE;
    }
    *result = pwd;
    return 0;
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    if (uid != 0) {
        if (result != 0) {
            *result = 0;
        }
        return 0;
    }
    return getpwnam_r("root", pwd, buf, buflen, result);
}

struct group *getgrnam(const char *name) {
    return name != 0 && strcmp(name, "root") == 0 ? &root_group : 0;
}

struct group *getgrgid(gid_t gid) {
    return gid == 0 ? &root_group : 0;
}

int getgrnam_r(const char *name, struct group *grp, char *buf, size_t buflen, struct group **result) {
    if (result == 0 || grp == 0 || buf == 0) {
        return EINVAL;
    }
    *result = 0;
    struct group *source = getgrnam(name);
    if (source == 0) {
        return 0;
    }
    char *cursor = buf;
    size_t remaining = buflen;
    *grp = *source;
    grp->gr_name = copy_string(&cursor, &remaining, source->gr_name);
    grp->gr_passwd = copy_string(&cursor, &remaining, source->gr_passwd);
    grp->gr_mem = 0;
    if (grp->gr_name == 0 || grp->gr_passwd == 0) {
        return ERANGE;
    }
    *result = grp;
    return 0;
}

int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen, struct group **result) {
    if (gid != 0) {
        if (result != 0) {
            *result = 0;
        }
        return 0;
    }
    return getgrnam_r("root", grp, buf, buflen, result);
}

uid_t getuid(void) {
    return 0;
}

uid_t geteuid(void) {
    return 0;
}

gid_t getgid(void) {
    return 0;
}

gid_t getegid(void) {
    return 0;
}

int setuid(uid_t uid) {
    if (uid != 0) {
        errno = EPERM;
        return -1;
    }
    return 0;
}

int seteuid(uid_t uid) {
    return setuid(uid);
}

int setgid(gid_t gid) {
    if (gid != 0) {
        errno = EPERM;
        return -1;
    }
    return 0;
}

int setegid(gid_t gid) {
    return setgid(gid);
}

int getgroups(int size, gid_t list[]) {
    if (size < 0) {
        errno = EINVAL;
        return -1;
    }
    if (size == 0) {
        return 1;
    }
    if (list == 0) {
        errno = EINVAL;
        return -1;
    }
    list[0] = 0;
    return 1;
}

int setgroups(size_t size, const gid_t *list) {
    for (size_t i = 0; i < size; i++) {
        if (list == 0 || list[i] != 0) {
            errno = EPERM;
            return -1;
        }
    }
    return 0;
}

int initgroups(const char *user, gid_t group) {
    if (user == 0 || group != 0) {
        errno = user == 0 ? EINVAL : EPERM;
        return -1;
    }
    return 0;
}
