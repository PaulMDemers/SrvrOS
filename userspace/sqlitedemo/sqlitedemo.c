#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sqlite3.h"

#include <srvros/sys.h>

#define SQLITEDEMO_PATH_MAX 160

struct srv_sqlite_file {
    sqlite3_file base;
    int fd;
    int delete_on_close;
    char path[SQLITEDEMO_PATH_MAX];
};

static int srv_sqlite_close(sqlite3_file *file) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    if (srv_file->fd >= 0) {
        close(srv_file->fd);
        srv_file->fd = -1;
    }
    if (srv_file->delete_on_close && srv_file->path[0] != '\0') {
        unlink(srv_file->path);
    }
    return SQLITE_OK;
}

static int srv_sqlite_read(sqlite3_file *file, void *buffer, int amount, sqlite3_int64 offset) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    unsigned char *out = (unsigned char *)buffer;
    int total = 0;
    while (total < amount) {
        ssize_t n = pread(srv_file->fd, out + total, (size_t)(amount - total), offset + total);
        if (n < 0) {
            return SQLITE_IOERR_READ;
        }
        if (n == 0) {
            memset(out + total, 0, (size_t)(amount - total));
            return SQLITE_IOERR_SHORT_READ;
        }
        total += (int)n;
    }
    return SQLITE_OK;
}

static int srv_sqlite_write(sqlite3_file *file, const void *buffer, int amount, sqlite3_int64 offset) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    const unsigned char *in = (const unsigned char *)buffer;
    int total = 0;
    while (total < amount) {
        ssize_t n = pwrite(srv_file->fd, in + total, (size_t)(amount - total), offset + total);
        if (n <= 0) {
            return SQLITE_IOERR_WRITE;
        }
        total += (int)n;
    }
    return SQLITE_OK;
}

static int srv_sqlite_truncate(sqlite3_file *file, sqlite3_int64 size) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    return ftruncate(srv_file->fd, (off_t)size) == 0 ? SQLITE_OK : SQLITE_IOERR_TRUNCATE;
}

static int srv_sqlite_sync(sqlite3_file *file, int flags) {
    (void)flags;
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    return fsync(srv_file->fd) == 0 ? SQLITE_OK : SQLITE_IOERR_FSYNC;
}

static int srv_sqlite_file_size(sqlite3_file *file, sqlite3_int64 *size) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    struct stat st;
    if (fstat(srv_file->fd, &st) != 0) {
        return SQLITE_IOERR_FSTAT;
    }
    *size = (sqlite3_int64)st.st_size;
    return SQLITE_OK;
}

static int srv_sqlite_lock(sqlite3_file *file, int lock) {
    (void)file;
    (void)lock;
    return SQLITE_OK;
}

static int srv_sqlite_unlock(sqlite3_file *file, int lock) {
    (void)file;
    (void)lock;
    return SQLITE_OK;
}

static int srv_sqlite_check_reserved_lock(sqlite3_file *file, int *reserved) {
    (void)file;
    *reserved = 0;
    return SQLITE_OK;
}

static int srv_sqlite_file_control(sqlite3_file *file, int op, void *arg) {
    (void)file;
    (void)op;
    (void)arg;
    return SQLITE_NOTFOUND;
}

static int srv_sqlite_sector_size(sqlite3_file *file) {
    (void)file;
    return 4096;
}

static int srv_sqlite_device_characteristics(sqlite3_file *file) {
    (void)file;
    return SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN;
}

static const sqlite3_io_methods srv_sqlite_io = {
    1,
    srv_sqlite_close,
    srv_sqlite_read,
    srv_sqlite_write,
    srv_sqlite_truncate,
    srv_sqlite_sync,
    srv_sqlite_file_size,
    srv_sqlite_lock,
    srv_sqlite_unlock,
    srv_sqlite_check_reserved_lock,
    srv_sqlite_file_control,
    srv_sqlite_sector_size,
    srv_sqlite_device_characteristics,
    0,
    0,
    0,
    0,
    0,
    0
};

static void copy_path(char *out, int out_size, const char *path) {
    int i = 0;
    if (out_size <= 0) {
        return;
    }
    while (path != 0 && path[i] != '\0' && i + 1 < out_size) {
        out[i] = path[i];
        i++;
    }
    out[i] = '\0';
}

static int srv_sqlite_open(sqlite3_vfs *vfs,
    const char *name,
    sqlite3_file *file,
    int flags,
    int *out_flags) {
    (void)vfs;
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    memset(srv_file, 0, sizeof(*srv_file));
    srv_file->fd = -1;

    const char *path = name != 0 ? name : "/fat/sqlite-temp.db";
    int open_flags = 0;
    if ((flags & SQLITE_OPEN_READWRITE) != 0) {
        open_flags |= O_RDWR;
    } else {
        open_flags |= O_RDONLY;
    }
    if ((flags & SQLITE_OPEN_CREATE) != 0) {
        open_flags |= O_CREAT;
    }
    if ((flags & SQLITE_OPEN_DELETEONCLOSE) != 0) {
        srv_file->delete_on_close = 1;
    }

    int fd = open(path, open_flags, 0666);
    if (fd < 0 && (flags & SQLITE_OPEN_READWRITE) != 0) {
        open_flags &= ~O_RDWR;
        open_flags |= O_RDONLY;
        fd = open(path, open_flags, 0666);
        if (out_flags != 0 && fd >= 0) {
            *out_flags = SQLITE_OPEN_READONLY;
        }
    } else if (out_flags != 0 && fd >= 0) {
        *out_flags = flags;
    }
    if (fd < 0) {
        return SQLITE_CANTOPEN;
    }

    srv_file->fd = fd;
    srv_file->base.pMethods = &srv_sqlite_io;
    copy_path(srv_file->path, sizeof(srv_file->path), path);
    return SQLITE_OK;
}

static int srv_sqlite_delete(sqlite3_vfs *vfs, const char *path, int sync_dir) {
    (void)vfs;
    (void)sync_dir;
    return unlink(path) == 0 || errno == ENOENT ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

static int srv_sqlite_access(sqlite3_vfs *vfs, const char *path, int flags, int *result) {
    (void)vfs;
    int mode = F_OK;
    if (flags == SQLITE_ACCESS_READWRITE) {
        mode = R_OK | W_OK;
    } else if (flags == SQLITE_ACCESS_READ) {
        mode = R_OK;
    }
    *result = access(path, mode) == 0;
    return SQLITE_OK;
}

static int srv_sqlite_full_pathname(sqlite3_vfs *vfs, const char *path, int out_size, char *out) {
    (void)vfs;
    if (path == 0 || path[0] == '\0') {
        copy_path(out, out_size, "/fat/sqlite-temp.db");
    } else if (path[0] == '/') {
        copy_path(out, out_size, path);
    } else {
        int written = snprintf(out, (size_t)out_size, "/fat/%s", path);
        if (written < 0 || written >= out_size) {
            return SQLITE_CANTOPEN;
        }
    }
    return SQLITE_OK;
}

static void *srv_sqlite_dl_open(sqlite3_vfs *vfs, const char *path) {
    (void)vfs;
    (void)path;
    return 0;
}

static void srv_sqlite_dl_error(sqlite3_vfs *vfs, int n, char *message) {
    (void)vfs;
    if (n > 0) {
        message[0] = '\0';
    }
}

static void (*srv_sqlite_dl_sym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void) {
    (void)vfs;
    (void)handle;
    (void)symbol;
    return 0;
}

static void srv_sqlite_dl_close(sqlite3_vfs *vfs, void *handle) {
    (void)vfs;
    (void)handle;
}

static int srv_sqlite_randomness(sqlite3_vfs *vfs, int n, char *out) {
    (void)vfs;
    uint64_t value = (uint64_t)srv_ticks() ^ 0x9e3779b97f4a7c15ull;
    for (int i = 0; i < n; i++) {
        value = value * 6364136223846793005ull + 1;
        out[i] = (char)(value >> 32);
    }
    return n;
}

static int srv_sqlite_sleep(sqlite3_vfs *vfs, int microseconds) {
    (void)vfs;
    if (microseconds > 0) {
        usleep((unsigned int)microseconds);
    }
    return microseconds;
}

static int srv_sqlite_current_time(sqlite3_vfs *vfs, double *now) {
    (void)vfs;
    *now = 2440587.5 + ((double)srv_ticks() / 100.0) / 86400.0;
    return SQLITE_OK;
}

static int srv_sqlite_get_last_error(sqlite3_vfs *vfs, int n, char *message) {
    (void)vfs;
    if (n > 0 && message != 0) {
        message[0] = '\0';
    }
    return 0;
}

static int srv_sqlite_current_time_int64(sqlite3_vfs *vfs, sqlite3_int64 *now) {
    (void)vfs;
    *now = 244058750000 + (sqlite3_int64)srv_ticks() * 10;
    return SQLITE_OK;
}

static sqlite3_vfs srv_sqlite_vfs = {
    3,
    sizeof(struct srv_sqlite_file),
    SQLITEDEMO_PATH_MAX,
    0,
    "srvros",
    0,
    srv_sqlite_open,
    srv_sqlite_delete,
    srv_sqlite_access,
    srv_sqlite_full_pathname,
    srv_sqlite_dl_open,
    srv_sqlite_dl_error,
    srv_sqlite_dl_sym,
    srv_sqlite_dl_close,
    srv_sqlite_randomness,
    srv_sqlite_sleep,
    srv_sqlite_current_time,
    srv_sqlite_get_last_error,
    srv_sqlite_current_time_int64,
    0,
    0,
    0
};

int sqlite3_os_init(void) {
    return sqlite3_vfs_register(&srv_sqlite_vfs, 1);
}

int sqlite3_os_end(void) {
    return SQLITE_OK;
}

static int exec_sql(sqlite3 *db, const char *sql) {
    char *error = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &error);
    if (rc != SQLITE_OK) {
        printf("sqlitedemo: sql failed rc=%d %s\n", rc, error != 0 ? error : "");
        sqlite3_free(error);
        return 0;
    }
    return 1;
}

int main(void) {
    puts("sqlitedemo: start");
    unlink("/fat/sqlitedemo.db");
    unlink("/fat/sqlitedemo.db-journal");

    sqlite3 *db = 0;
    int rc = sqlite3_open_v2("/fat/sqlitedemo.db", &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "srvros");
    if (rc != SQLITE_OK) {
        printf("sqlitedemo: open failed rc=%d\n", rc);
        sqlite3_close(db);
        return 1;
    }
    if (!exec_sql(db, "PRAGMA journal_mode=DELETE;")) {
        sqlite3_close(db);
        return 2;
    }
    if (!exec_sql(db, "CREATE TABLE pages(path TEXT PRIMARY KEY, body TEXT);")) {
        sqlite3_close(db);
        return 3;
    }
    if (!exec_sql(db, "INSERT INTO pages VALUES('/','hello from sqlite');")) {
        sqlite3_close(db);
        return 4;
    }
    if (!exec_sql(db, "INSERT INTO pages VALUES('/status','ok');")) {
        sqlite3_close(db);
        return 5;
    }
    sqlite3_close(db);

    rc = sqlite3_open_v2("/fat/sqlitedemo.db", &db, SQLITE_OPEN_READWRITE, "srvros");
    if (rc != SQLITE_OK) {
        printf("sqlitedemo: reopen failed rc=%d\n", rc);
        sqlite3_close(db);
        return 6;
    }

    sqlite3_stmt *stmt = 0;
    rc = sqlite3_prepare_v2(db,
        "SELECT body FROM pages WHERE path=?1;",
        -1,
        &stmt,
        0);
    if (rc != SQLITE_OK) {
        printf("sqlitedemo: prepare failed rc=%d\n", rc);
        sqlite3_close(db);
        return 7;
    }
    sqlite3_bind_text(stmt, 1, "/", -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        printf("sqlitedemo: step failed rc=%d\n", rc);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 8;
    }
    const unsigned char *body = sqlite3_column_text(stmt, 0);
    if (body == 0 || strcmp((const char *)body, "hello from sqlite") != 0) {
        puts("sqlitedemo: query mismatch");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 9;
    }
    sqlite3_finalize(stmt);
    puts("sqlitedemo: query ok");

    rc = sqlite3_prepare_v2(db, "SELECT count(*) FROM pages;", -1, &stmt, 0);
    if (rc != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW || sqlite3_column_int(stmt, 0) != 2) {
        puts("sqlitedemo: count failed");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 10;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    struct stat st;
    if (stat("/fat/sqlitedemo.db", &st) != 0 || st.st_size == 0) {
        puts("sqlitedemo: db file missing");
        return 11;
    }
    printf("sqlitedemo: db size=%llu\n", (unsigned long long)st.st_size);
    printf("sqlitedemo: ok sqlite %s\n", sqlite3_libversion());
    return 0;
}
