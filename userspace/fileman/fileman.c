#include <srvros/cli.h>
#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define WIDTH 760
#define HEIGHT 470
#define ENTRY_MAX 64
#define NAME_MAX 64
#define COPY_BUFFER 2048
#define RECURSIVE_MAX 160

enum fileman_sort_column {
    FILEMAN_SORT_NAME = 0,
    FILEMAN_SORT_SIZE = 1,
};

struct file_entry {
    char name[NAME_MAX];
    char path[CLI_PATH_MAX];
    char detail[32];
    uint64_t size;
    int dir;
};

struct recursive_entry {
    char path[CLI_PATH_MAX];
    char rel[CLI_PATH_MAX];
    int dir;
};

struct op_stats {
    uint64_t files;
    uint64_t dirs;
};

enum fileman_op_kind {
    FILEMAN_OP_NONE = 0,
    FILEMAN_OP_COPY,
    FILEMAN_OP_MOVE,
    FILEMAN_OP_DELETE,
};

enum fileman_op_phase {
    FILEMAN_OP_IDLE = 0,
    FILEMAN_OP_COPY_ROOT,
    FILEMAN_OP_COPY_DIRS,
    FILEMAN_OP_COPY_FILES,
    FILEMAN_OP_DELETE_FILES,
    FILEMAN_OP_DELETE_DIRS,
    FILEMAN_OP_DELETE_ROOT,
};

struct fileman_operation {
    enum fileman_op_kind kind;
    enum fileman_op_phase phase;
    struct recursive_entry entries[RECURSIVE_MAX];
    uint64_t count;
    uint64_t index;
    struct op_stats stats;
    char source[CLI_PATH_MAX];
    char dest[CLI_PATH_MAX];
    char copy_target[CLI_PATH_MAX];
    char select_name[NAME_MAX];
    const char *verb;
    int copy_in_fd;
    int copy_out_fd;
    int copy_open;
    int active;
    int source_is_dir;
};

static char copy_buffer[COPY_BUFFER];
static const struct gui2_list_column file_columns[] = {
    { "NAME", 540 },
    { "SIZE", 110 },
};

static void print_u64(uint64_t value) {
    char digits[21];
    uint64_t count = 0;
    if (value == 0) {
        srv_puts("0");
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        char text[2] = { digits[--count], '\0' };
        srv_puts(text);
    }
}

static void append_char(char *out, uint64_t capacity, uint64_t *length, char c) {
    if (*length + 1 >= capacity) {
        return;
    }
    out[(*length)++] = c;
    out[*length] = '\0';
}

static void append_u64(char *out, uint64_t capacity, uint64_t *length, uint64_t value) {
    char digits[21];
    uint64_t count = 0;
    if (value == 0) {
        append_char(out, capacity, length, '0');
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        append_char(out, capacity, length, digits[--count]);
    }
}

static int path_is_directory(const char *path) {
    struct srv_stat info;
    return srv_stat(path, &info) == 0 && info.type == 1;
}

static int has_ext(const char *path, const char *ext) {
    uint64_t path_len = cli_strlen(path);
    uint64_t ext_len = cli_strlen(ext);
    if (path_len < ext_len) {
        return 0;
    }
    const char *p = path + path_len - ext_len;
    for (uint64_t i = 0; i < ext_len; i++) {
        char a = p[i];
        char b = ext[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int has_text_ext(const char *path) {
    static const char *exts[] = {
        ".txt", ".md", ".log", ".ini", ".cfg", ".conf", ".json", ".csv",
        ".c", ".h", ".cc", ".cpp", ".hpp", ".s", ".asm", ".ld",
        ".sh", ".lua", ".js", ".html", ".htm", ".css", ".xml",
        ".toml", ".yaml", ".yml",
    };
    for (uint64_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        if (has_ext(path, exts[i])) {
            return 1;
        }
    }
    return 0;
}

static int fileman_open_app(const char *path, const char **app_out,
    const char **label_out) {
    if (app_out != 0) {
        *app_out = 0;
    }
    if (label_out != 0) {
        *label_out = 0;
    }
    if (path == 0 || path[0] == '\0') {
        return 0;
    }
    if (has_ext(path, ".bmp")) {
        if (app_out != 0) {
            *app_out = "/fat/bin/paint";
        }
        if (label_out != 0) {
            *label_out = "PAINT";
        }
        return 1;
    }
    if (has_text_ext(path)) {
        if (app_out != 0) {
            *app_out = "/fat/bin/textedit";
        }
        if (label_out != 0) {
            *label_out = "TEXT EDIT";
        }
        return 1;
    }
    return 0;
}

static int entry_seen(const struct file_entry *entries, uint64_t count, const char *name) {
    for (uint64_t i = 0; i < count; i++) {
        if (cli_streq(entries[i].name, name)) {
            return 1;
        }
    }
    return 0;
}

static int cmp_char(char a, char b) {
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
    }
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static int cmp_text(const char *a, const char *b) {
    uint64_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        int result = cmp_char(a[i], b[i]);
        if (result != 0) {
            return result;
        }
        i++;
    }
    if (a[i] == '\0' && b[i] == '\0') {
        return 0;
    }
    return a[i] == '\0' ? -1 : 1;
}

static int entry_compare(const struct file_entry *a, const struct file_entry *b,
    size_t sort_column, int sort_desc) {
    int result;
    if (a->dir != b->dir) {
        return a->dir ? -1 : 1;
    }
    if (sort_column == FILEMAN_SORT_SIZE && a->size != b->size) {
        result = a->size < b->size ? -1 : 1;
    } else {
        result = cmp_text(a->name, b->name);
    }
    if (result == 0) {
        result = cmp_text(a->name, b->name);
    }
    return sort_desc ? -result : result;
}

static void sort_entries(struct file_entry *entries, uint64_t count,
    size_t sort_column, int sort_desc) {
    for (uint64_t i = 1; i < count; i++) {
        struct file_entry value = entries[i];
        uint64_t j = i;
        while (j > 0 && entry_compare(&value, &entries[j - 1],
                sort_column, sort_desc) < 0) {
            entries[j] = entries[j - 1];
            j--;
        }
        entries[j] = value;
    }
}

static void bind_items(const struct file_entry *entries,
    struct gui2_list_item *items, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        items[i].label = entries[i].name;
        items[i].detail = entries[i].detail;
        items[i].flags = entries[i].dir ? GUI2_LIST_ITEM_DIR : 0;
    }
}

static void parent_path(char *out, uint64_t capacity, const char *path) {
    uint64_t len;
    cli_copy(out, capacity, path);
    len = cli_strlen(out);
    while (len > 1 && out[len - 1] == '/') {
        out[--len] = '\0';
    }
    while (len > 1 && out[len - 1] != '/') {
        out[--len] = '\0';
    }
    if (len > 1 && out[len - 1] == '/') {
        out[--len] = '\0';
    }
    if (out[0] == '\0') {
        cli_copy(out, capacity, "/");
    }
}

static int copy_file(const char *source, const char *dest) {
    int in_fd = (int)srv_open(source);
    int out_fd;
    if (in_fd < 0) {
        return -1;
    }
    out_fd = (int)srv_open_mode(dest, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
    if (out_fd < 0) {
        srv_close(in_fd);
        return -1;
    }
    for (;;) {
        long count = srv_read(in_fd, copy_buffer, sizeof(copy_buffer));
        if (count < 0) {
            srv_close(in_fd);
            srv_close(out_fd);
            return -1;
        }
        if (count == 0) {
            break;
        }
        if (srv_write(out_fd, copy_buffer, (size_t)count) != count) {
            srv_close(in_fd);
            srv_close(out_fd);
            return -1;
        }
    }
    srv_close(in_fd);
    return srv_close(out_fd) < 0 ? -1 : 0;
}

static int recursive_entry_compare_depth(const struct recursive_entry *a,
    const struct recursive_entry *b, int descending) {
    size_t alen = cli_strlen(a->rel);
    size_t blen = cli_strlen(b->rel);
    if (alen == blen) {
        return cmp_text(a->rel, b->rel);
    }
    if (descending) {
        return alen > blen ? -1 : 1;
    }
    return alen < blen ? -1 : 1;
}

static void sort_recursive_entries(struct recursive_entry *entries,
    uint64_t count, int descending) {
    for (uint64_t i = 1; i < count; i++) {
        struct recursive_entry value = entries[i];
        uint64_t j = i;
        while (j > 0 && recursive_entry_compare_depth(&value, &entries[j - 1],
                descending) < 0) {
            entries[j] = entries[j - 1];
            j--;
        }
        entries[j] = value;
    }
}

static int path_is_within(const char *path, const char *possible_parent) {
    char prefix[CLI_PATH_MAX];
    cli_join_path(prefix, sizeof(prefix), possible_parent, "");
    return cli_starts_with(path, prefix);
}

static int collect_recursive(const char *source, struct recursive_entry *entries,
    uint64_t capacity, uint64_t *count_out) {
    char prefix[CLI_PATH_MAX];
    char listed[CLI_PATH_MAX];
    uint64_t size = 0;
    uint64_t count = 0;
    if (entries == 0 || count_out == 0 || !path_is_directory(source)) {
        return -1;
    }
    cli_join_path(prefix, sizeof(prefix), source, "");
    for (uint64_t i = 0;; i++) {
        long result = srv_list(i, listed, sizeof(listed), &size);
        if (result <= 0) {
            break;
        }
        if (!cli_starts_with(listed, prefix)) {
            continue;
        }
        if (count >= capacity) {
            return -1;
        }
        cli_copy(entries[count].path, sizeof(entries[count].path), listed);
        cli_copy(entries[count].rel, sizeof(entries[count].rel),
            listed + cli_strlen(prefix));
        if (entries[count].rel[0] == '\0') {
            continue;
        }
        entries[count].dir = path_is_directory(listed);
        count++;
    }
    *count_out = count;
    return 0;
}

static int recursive_dest_path(char *out, uint64_t capacity,
    const char *dest_root, const char *rel) {
    if (rel == 0 || rel[0] == '\0') {
        cli_copy(out, capacity, dest_root);
        return 1;
    }
    return cli_join_path(out, capacity, dest_root, rel);
}

static int copy_tree(const char *source, const char *dest, struct op_stats *stats) {
    struct recursive_entry collected[RECURSIVE_MAX];
    uint64_t count = 0;
    char target[CLI_PATH_MAX];
    if (stats != 0) {
        stats->files = 0;
        stats->dirs = 0;
    }
    if (cli_streq(source, dest)) {
        return -1;
    }
    if (!path_is_directory(source)) {
        int ok = copy_file(source, dest) == 0;
        if (ok && stats != 0) {
            stats->files = 1;
        }
        return ok ? 0 : -1;
    }
    if (path_is_within(dest, source)) {
        return -1;
    }
    if (collect_recursive(source, collected, RECURSIVE_MAX, &count) != 0) {
        return -1;
    }
    if (srv_mkdir(dest) < 0 && !path_is_directory(dest)) {
        return -1;
    }
    if (stats != 0) {
        stats->dirs++;
    }
    sort_recursive_entries(collected, count, 0);
    for (uint64_t i = 0; i < count; i++) {
        if (!collected[i].dir) {
            continue;
        }
        if (!recursive_dest_path(target, sizeof(target), dest, collected[i].rel) ||
            (srv_mkdir(target) < 0 && !path_is_directory(target))) {
            return -1;
        }
        if (stats != 0) {
            stats->dirs++;
        }
    }
    for (uint64_t i = 0; i < count; i++) {
        if (collected[i].dir) {
            continue;
        }
        if (!recursive_dest_path(target, sizeof(target), dest, collected[i].rel) ||
            copy_file(collected[i].path, target) != 0) {
            return -1;
        }
        if (stats != 0) {
            stats->files++;
        }
    }
    return 0;
}

static int delete_tree(const char *source, struct op_stats *stats) {
    struct recursive_entry collected[RECURSIVE_MAX];
    uint64_t count = 0;
    if (stats != 0) {
        stats->files = 0;
        stats->dirs = 0;
    }
    if (!path_is_directory(source)) {
        int ok = srv_unlink(source) == 0;
        if (ok && stats != 0) {
            stats->files = 1;
        }
        return ok ? 0 : -1;
    }
    if (collect_recursive(source, collected, RECURSIVE_MAX, &count) != 0) {
        return -1;
    }
    for (uint64_t i = 0; i < count; i++) {
        if (collected[i].dir) {
            continue;
        }
        if (srv_unlink(collected[i].path) != 0) {
            return -1;
        }
        if (stats != 0) {
            stats->files++;
        }
    }
    sort_recursive_entries(collected, count, 1);
    for (uint64_t i = 0; i < count; i++) {
        if (!collected[i].dir) {
            continue;
        }
        if (srv_rmdir(collected[i].path) != 0) {
            return -1;
        }
        if (stats != 0) {
            stats->dirs++;
        }
    }
    if (srv_rmdir(source) != 0) {
        return -1;
    }
    if (stats != 0) {
        stats->dirs++;
    }
    return 0;
}

static int move_tree(const char *source, const char *dest, struct op_stats *stats) {
    if (cli_streq(source, dest)) {
        return -1;
    }
    if (srv_rename(source, dest) == 0) {
        if (stats != 0) {
            stats->files = path_is_directory(dest) ? 0 : 1;
            stats->dirs = path_is_directory(dest) ? 1 : 0;
        }
        return 0;
    }
    if (copy_tree(source, dest, stats) != 0) {
        return -1;
    }
    return delete_tree(source, 0);
}

static void format_op_status(char *out, uint64_t capacity, const char *verb,
    const struct op_stats *stats) {
    uint64_t length = 0;
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    while (verb != 0 && *verb != '\0') {
        append_char(out, capacity, &length, *verb++);
    }
    append_char(out, capacity, &length, ' ');
    if (stats != 0) {
        append_u64(out, capacity, &length, stats->files);
        append_char(out, capacity, &length, 'F');
        append_char(out, capacity, &length, '/');
        append_u64(out, capacity, &length, stats->dirs);
        append_char(out, capacity, &length, 'D');
    } else {
        append_char(out, capacity, &length, 'O');
        append_char(out, capacity, &length, 'K');
    }
}

static void operation_reset(struct fileman_operation *operation) {
    if (operation == 0) {
        return;
    }
    if (operation->copy_open) {
        if (operation->copy_in_fd >= 0) {
            srv_close(operation->copy_in_fd);
        }
        if (operation->copy_out_fd >= 0) {
            srv_close(operation->copy_out_fd);
        }
    }
    operation->kind = FILEMAN_OP_NONE;
    operation->phase = FILEMAN_OP_IDLE;
    operation->count = 0;
    operation->index = 0;
    operation->stats.files = 0;
    operation->stats.dirs = 0;
    operation->source[0] = '\0';
    operation->dest[0] = '\0';
    operation->copy_target[0] = '\0';
    operation->select_name[0] = '\0';
    operation->verb = "DONE";
    operation->copy_in_fd = -1;
    operation->copy_out_fd = -1;
    operation->copy_open = 0;
    operation->active = 0;
    operation->source_is_dir = 0;
}

static void operation_close_copy(struct fileman_operation *operation) {
    if (operation == 0 || !operation->copy_open) {
        return;
    }
    if (operation->copy_in_fd >= 0) {
        srv_close(operation->copy_in_fd);
    }
    if (operation->copy_out_fd >= 0) {
        srv_close(operation->copy_out_fd);
    }
    operation->copy_in_fd = -1;
    operation->copy_out_fd = -1;
    operation->copy_open = 0;
    operation->copy_target[0] = '\0';
}

static void operation_format_progress(const struct fileman_operation *operation,
    char *out, uint64_t capacity) {
    uint64_t length = 0;
    const char *verb = "WORKING";
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    if (operation != 0) {
        if (operation->kind == FILEMAN_OP_COPY) {
            verb = "COPYING";
        } else if (operation->kind == FILEMAN_OP_MOVE) {
            verb = "MOVING";
        } else if (operation->kind == FILEMAN_OP_DELETE) {
            verb = "DELETING";
        }
    }
    while (*verb != '\0') {
        append_char(out, capacity, &length, *verb++);
    }
    append_char(out, capacity, &length, ' ');
    append_u64(out, capacity, &length, operation != 0 ? operation->index : 0);
    append_char(out, capacity, &length, '/');
    append_u64(out, capacity, &length, operation != 0 ? operation->count : 0);
    append_char(out, capacity, &length, ' ');
    append_u64(out, capacity, &length,
        operation != 0 ? operation->stats.files : 0);
    append_char(out, capacity, &length, 'F');
    append_char(out, capacity, &length, '/');
    append_u64(out, capacity, &length,
        operation != 0 ? operation->stats.dirs : 0);
    append_char(out, capacity, &length, 'D');
}

static uint64_t operation_progress_value(const struct fileman_operation *operation) {
    if (operation == 0) {
        return 0;
    }
    return operation->index > operation->count ? operation->count : operation->index;
}

static uint64_t operation_progress_max(const struct fileman_operation *operation) {
    if (operation == 0 || operation->count == 0) {
        return 1;
    }
    return operation->count;
}

static const char *operation_progress_title(const struct fileman_operation *operation) {
    if (operation == 0) {
        return "WORKING";
    }
    if (operation->kind == FILEMAN_OP_COPY) {
        return "COPYING";
    }
    if (operation->kind == FILEMAN_OP_MOVE) {
        return "MOVING";
    }
    if (operation->kind == FILEMAN_OP_DELETE) {
        return "DELETING";
    }
    return "WORKING";
}

static int operation_start_copy(struct fileman_operation *operation,
    enum fileman_op_kind kind, const char *source, const char *dest,
    const char *select_name) {
    uint64_t count = 0;
    if (operation == 0 || source == 0 || dest == 0 ||
        (kind != FILEMAN_OP_COPY && kind != FILEMAN_OP_MOVE)) {
        return -1;
    }
    if (cli_streq(source, dest)) {
        return -1;
    }
    operation_reset(operation);
    operation->kind = kind;
    operation->verb = kind == FILEMAN_OP_MOVE ? "MOVED" : "COPIED";
    operation->active = 1;
    cli_copy(operation->source, sizeof(operation->source), source);
    cli_copy(operation->dest, sizeof(operation->dest), dest);
    if (select_name != 0) {
        cli_copy(operation->select_name, sizeof(operation->select_name), select_name);
    }
    operation->source_is_dir = path_is_directory(source);
    if (!operation->source_is_dir) {
        operation->phase = FILEMAN_OP_COPY_FILES;
        operation->count = 1;
        cli_copy(operation->entries[0].path, sizeof(operation->entries[0].path), source);
        operation->entries[0].rel[0] = '\0';
        operation->entries[0].dir = 0;
        return 0;
    }
    if (path_is_within(dest, source)) {
        operation_reset(operation);
        return -1;
    }
    if (collect_recursive(source, operation->entries, RECURSIVE_MAX, &count) != 0) {
        operation_reset(operation);
        return -1;
    }
    sort_recursive_entries(operation->entries, count, 0);
    operation->count = count + 1;
    operation->phase = FILEMAN_OP_COPY_ROOT;
    return 0;
}

static int operation_start_delete(struct fileman_operation *operation,
    const char *source, const char *select_name) {
    uint64_t count = 0;
    if (operation == 0 || source == 0) {
        return -1;
    }
    operation_reset(operation);
    operation->kind = FILEMAN_OP_DELETE;
    operation->verb = "DELETED";
    operation->active = 1;
    cli_copy(operation->source, sizeof(operation->source), source);
    if (select_name != 0) {
        cli_copy(operation->select_name, sizeof(operation->select_name), select_name);
    }
    operation->source_is_dir = path_is_directory(source);
    if (!operation->source_is_dir) {
        operation->phase = FILEMAN_OP_DELETE_ROOT;
        operation->count = 1;
        return 0;
    }
    if (collect_recursive(source, operation->entries, RECURSIVE_MAX, &count) != 0) {
        operation_reset(operation);
        return -1;
    }
    sort_recursive_entries(operation->entries, count, 1);
    operation->count = count + 1;
    operation->phase = FILEMAN_OP_DELETE_FILES;
    return 0;
}

static int operation_copy_file_chunk(struct fileman_operation *operation,
    const char *source, const char *target) {
    long count;
    if (operation == 0 || source == 0 || target == 0) {
        return -1;
    }
    if (!operation->copy_open) {
        operation->copy_in_fd = (int)srv_open(source);
        if (operation->copy_in_fd < 0) {
            return -1;
        }
        operation->copy_out_fd = (int)srv_open_mode(target,
            SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
        if (operation->copy_out_fd < 0) {
            operation_close_copy(operation);
            return -1;
        }
        operation->copy_open = 1;
        cli_copy(operation->copy_target, sizeof(operation->copy_target), target);
    } else if (!cli_streq(operation->copy_target, target)) {
        return -1;
    }

    count = srv_read(operation->copy_in_fd, copy_buffer, sizeof(copy_buffer));
    if (count < 0) {
        operation_close_copy(operation);
        return -1;
    }
    if (count == 0) {
        operation_close_copy(operation);
        return 1;
    }
    if (srv_write(operation->copy_out_fd, copy_buffer, (size_t)count) != count) {
        operation_close_copy(operation);
        return -1;
    }
    return 0;
}

static int operation_step_copy(struct fileman_operation *operation,
    char *status, uint64_t status_capacity) {
    char target[CLI_PATH_MAX];
    if (operation->phase == FILEMAN_OP_COPY_ROOT) {
        if (srv_mkdir(operation->dest) < 0 && !path_is_directory(operation->dest)) {
            cli_copy(status, status_capacity, "COPY FAILED");
            return -1;
        }
        operation->stats.dirs++;
        operation->index = 0;
        operation->phase = FILEMAN_OP_COPY_DIRS;
        operation_format_progress(operation, status, status_capacity);
        return 0;
    }
    if (operation->phase == FILEMAN_OP_COPY_DIRS) {
        while (operation->index < operation->count - 1) {
            struct recursive_entry *entry = &operation->entries[operation->index++];
            if (!entry->dir) {
                continue;
            }
            if (!recursive_dest_path(target, sizeof(target), operation->dest, entry->rel) ||
                (srv_mkdir(target) < 0 && !path_is_directory(target))) {
                cli_copy(status, status_capacity, "COPY FAILED");
                return -1;
            }
            operation->stats.dirs++;
            operation_format_progress(operation, status, status_capacity);
            return 0;
        }
        operation->index = 0;
        operation->phase = FILEMAN_OP_COPY_FILES;
    }
    if (operation->phase == FILEMAN_OP_COPY_FILES) {
        while (operation->index < (operation->source_is_dir ?
                operation->count - 1 : operation->count)) {
            int chunk_result;
            struct recursive_entry *entry = &operation->entries[operation->index];
            if (entry->dir) {
                operation->index++;
                continue;
            }
            if (operation->source_is_dir) {
                if (!recursive_dest_path(target, sizeof(target), operation->dest,
                        entry->rel)) {
                    cli_copy(status, status_capacity, "COPY FAILED");
                    return -1;
                }
            } else {
                cli_copy(target, sizeof(target), operation->dest);
            }
            chunk_result = operation_copy_file_chunk(operation, entry->path, target);
            if (chunk_result < 0) {
                cli_copy(status, status_capacity, "COPY FAILED");
                return -1;
            }
            if (chunk_result == 0) {
                operation_format_progress(operation, status, status_capacity);
                return 0;
            }
            operation->index++;
            operation->stats.files++;
            operation_format_progress(operation, status, status_capacity);
            return 0;
        }
        if (operation->kind != FILEMAN_OP_MOVE) {
            format_op_status(status, status_capacity, operation->verb, &operation->stats);
            return 1;
        }
        if (!operation->source_is_dir) {
            operation->phase = FILEMAN_OP_DELETE_ROOT;
            operation->index = 0;
            operation->count = 1;
        } else {
            sort_recursive_entries(operation->entries, operation->count - 1, 1);
            operation->phase = FILEMAN_OP_DELETE_FILES;
            operation->index = 0;
        }
        operation_format_progress(operation, status, status_capacity);
        return 0;
    }
    return 0;
}

static int operation_step_delete(struct fileman_operation *operation,
    char *status, uint64_t status_capacity) {
    if (operation->phase == FILEMAN_OP_DELETE_FILES) {
        while (operation->index < operation->count - 1) {
            struct recursive_entry *entry = &operation->entries[operation->index++];
            if (entry->dir) {
                continue;
            }
            if (srv_unlink(entry->path) != 0) {
                cli_copy(status, status_capacity, "DELETE FAILED");
                return -1;
            }
            if (operation->kind == FILEMAN_OP_DELETE) {
                operation->stats.files++;
            }
            operation_format_progress(operation, status, status_capacity);
            return 0;
        }
        operation->index = 0;
        operation->phase = FILEMAN_OP_DELETE_DIRS;
    }
    if (operation->phase == FILEMAN_OP_DELETE_DIRS) {
        while (operation->index < operation->count - 1) {
            struct recursive_entry *entry = &operation->entries[operation->index++];
            if (!entry->dir) {
                continue;
            }
            if (srv_rmdir(entry->path) != 0) {
                cli_copy(status, status_capacity, "DELETE FAILED");
                return -1;
            }
            if (operation->kind == FILEMAN_OP_DELETE) {
                operation->stats.dirs++;
            }
            operation_format_progress(operation, status, status_capacity);
            return 0;
        }
        operation->phase = FILEMAN_OP_DELETE_ROOT;
    }
    if (operation->phase == FILEMAN_OP_DELETE_ROOT) {
        int ok;
        if (operation->source_is_dir) {
            ok = srv_rmdir(operation->source) == 0;
            if (ok && operation->kind == FILEMAN_OP_DELETE) {
                operation->stats.dirs++;
            }
        } else {
            ok = srv_unlink(operation->source) == 0;
            if (ok && operation->kind == FILEMAN_OP_DELETE) {
                operation->stats.files++;
            }
        }
        if (!ok) {
            cli_copy(status, status_capacity, "DELETE FAILED");
            return -1;
        }
        format_op_status(status, status_capacity, operation->verb, &operation->stats);
        return 1;
    }
    return 0;
}

static int operation_step(struct fileman_operation *operation,
    char *status, uint64_t status_capacity) {
    if (operation == 0 || !operation->active) {
        return 0;
    }
    if (operation->phase == FILEMAN_OP_COPY_ROOT ||
        operation->phase == FILEMAN_OP_COPY_DIRS ||
        operation->phase == FILEMAN_OP_COPY_FILES) {
        return operation_step_copy(operation, status, status_capacity);
    }
    return operation_step_delete(operation, status, status_capacity);
}

static void update_progress_dialog(struct gui2_dialog *dialog,
    const struct fileman_operation *operation, const char *status) {
    if (dialog == 0 || operation == 0 || !operation->active) {
        return;
    }
    if (!dialog->active) {
        gui2_dialog_open(dialog, operation_progress_title(operation), "PLEASE WAIT");
    } else {
        dialog->title = operation_progress_title(operation);
    }
    gui2_dialog_set_progress(dialog,
        operation_progress_value(operation),
        operation_progress_max(operation),
        status);
}

static void format_detail(char *out, uint64_t capacity, uint64_t size, int dir) {
    uint64_t length = 0;
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    if (dir) {
        cli_copy(out, capacity, "DIR");
        return;
    }
    append_u64(out, capacity, &length, size);
    append_char(out, capacity, &length, 'B');
}

static int load_entries(const char *cwd, struct file_entry *entries,
    struct gui2_list_item *items, uint64_t capacity, uint64_t *count_out) {
    char prefix[CLI_PATH_MAX];
    char listed[CLI_PATH_MAX];
    uint64_t size = 0;
    uint64_t count = 0;
    int found = 0;

    (void)items;
    if (count_out == 0) {
        return -1;
    }
    *count_out = 0;
    if (!path_is_directory(cwd)) {
        return -1;
    }
    cli_join_path(prefix, sizeof(prefix), cwd, "");
    for (uint64_t i = 0;; i++) {
        long result = srv_list(i, listed, sizeof(listed), &size);
        if (result <= 0) {
            break;
        }
        if (cli_streq(listed, cwd)) {
            found = 1;
            continue;
        }
        if (!cli_starts_with(listed, prefix)) {
            continue;
        }

        const char *rest = listed + cli_strlen(prefix);
        char child[NAME_MAX];
        uint64_t child_len = 0;
        int dir;
        while (rest[child_len] != '\0' && rest[child_len] != '/' &&
            child_len + 1 < sizeof(child)) {
            child[child_len] = rest[child_len];
            child_len++;
        }
        child[child_len] = '\0';
        if (child[0] == '\0' || entry_seen(entries, count, child)) {
            continue;
        }
        if (count >= capacity) {
            break;
        }
        dir = rest[child_len] == '/';
        cli_copy(entries[count].name, sizeof(entries[count].name), child);
        cli_join_path(entries[count].path, sizeof(entries[count].path), cwd, child);
        entries[count].size = size;
        entries[count].dir = dir || path_is_directory(entries[count].path);
        format_detail(entries[count].detail, sizeof(entries[count].detail),
            entries[count].size, entries[count].dir);
        count++;
        found = 1;
    }
    *count_out = count;
    return found ? 0 : -1;
}

static void refresh_fileman(struct gui2_list *list, const char *cwd,
    struct file_entry *entries, struct gui2_list_item *items,
    uint64_t *entry_count, char *status, uint64_t status_capacity) {
    if (load_entries(cwd, entries, items, ENTRY_MAX, entry_count) == 0) {
        sort_entries(entries, *entry_count, list->sort_column, list->sort_desc);
        bind_items(entries, items, *entry_count);
        cli_copy(status, status_capacity, *entry_count == 0 ? "EMPTY" : "READY");
    } else {
        *entry_count = 0;
        cli_copy(status, status_capacity, "EMPTY OR UNAVAILABLE");
    }
    gui2_list_set_items(list, items, (size_t)*entry_count);
}

static void refresh_fileman_select(struct gui2_list *list, const char *cwd,
    struct file_entry *entries, struct gui2_list_item *items,
    uint64_t *entry_count, char *status, uint64_t status_capacity,
    const char *selected_name) {
    refresh_fileman(list, cwd, entries, items, entry_count, status, status_capacity);
    if (selected_name != 0 && selected_name[0] != '\0') {
        for (uint64_t i = 0; i < *entry_count; i++) {
            if (cli_streq(entries[i].name, selected_name)) {
                list->selected = i;
                gui2_list_keep_selected_visible(list);
                break;
            }
        }
    }
}

static void selected_path(char *out, uint64_t capacity, const char *cwd,
    const struct file_entry *entries, uint64_t count, uint64_t selected,
    const char *name) {
    if (name != 0 && name[0] != '\0') {
        cli_join_path(out, capacity, cwd, name);
    } else if (count != 0 && selected < count) {
        cli_copy(out, capacity, entries[selected].path);
    } else {
        cli_copy(out, capacity, cwd);
    }
}

static void draw_fileman(struct gui2_window *window,
    struct gui2_list *list,
    struct gui2_textbox *name_box,
    struct gui2_button *up,
    struct gui2_button *open,
    struct gui2_button *newdir,
    struct gui2_button *rename,
    struct gui2_button *del,
    struct gui2_button *copy,
    struct gui2_button *move,
    struct gui2_button *refresh,
    const char *cwd,
    const char *status) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    uint64_t gap = theme->gap;
    uint64_t content_w = window->width > 2 * pad ? window->width - 2 * pad : 1;
    uint64_t reserved = theme->toolbar_h + theme->status_h + 4 * pad +
        2 * theme->control_h + 2 * gap;
    uint64_t list_h = window->height > reserved ? window->height - reserved : 90;
    int64_t list_y = (int64_t)(theme->toolbar_h + pad);
    int64_t name_y = list_y + (int64_t)list_h + (int64_t)gap;
    int64_t buttons_y = name_y + (int64_t)theme->control_h + (int64_t)gap;

    gui2_clear(window, theme->canvas);
    gui2_app_header(window, "FILES", cwd);

    list->x = (int64_t)pad;
    list->y = list_y;
    list->width = content_w;
    list->height = list_h;
    gui2_list_draw(window, list);

    name_box->x = (int64_t)pad;
    name_box->y = name_y;
    name_box->width = content_w;
    name_box->height = theme->control_h;
    gui2_textbox_draw(window, name_box);

    struct gui2_button buttons[] = {
        *up, *open, *newdir, *rename, *del, *copy, *move, *refresh
    };
    gui2_layout_button_row(buttons, 8, (int64_t)pad, buttons_y,
        content_w, theme->control_h, gap);
    *up = buttons[0];
    *open = buttons[1];
    *newdir = buttons[2];
    *rename = buttons[3];
    *del = buttons[4];
    *copy = buttons[5];
    *move = buttons[6];
    *refresh = buttons[7];

    gui2_button_draw(window, up);
    gui2_button_draw(window, open);
    gui2_button_draw(window, newdir);
    gui2_button_draw(window, rename);
    gui2_button_draw(window, del);
    gui2_button_draw(window, copy);
    gui2_button_draw(window, move);
    gui2_button_draw(window, refresh);
    gui2_status_bar(window, status != 0 ? status : "READY", cwd);
}

static void confirm_message(char *out, uint64_t capacity, const char *name) {
    uint64_t length = 0;
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    const char *prefix = "DELETE ";
    while (*prefix != '\0') {
        append_char(out, capacity, &length, *prefix++);
    }
    for (const char *p = name; p != 0 && *p != '\0'; p++) {
        append_char(out, capacity, &length, *p);
    }
    append_char(out, capacity, &length, '?');
}

static void clear_name(struct gui2_textbox *name_box) {
    if (name_box == 0 || name_box->buffer == 0 || name_box->capacity == 0) {
        return;
    }
    name_box->buffer[0] = '\0';
    name_box->length = 0;
    name_box->cursor = 0;
}

static int run_operation_to_completion(struct fileman_operation *operation) {
    char status[80];
    uint64_t spins = 0;
    while (operation->active && spins++ < 512) {
        int result = operation_step(operation, status, sizeof(status));
        if (result < 0) {
            operation_reset(operation);
            return -1;
        }
        if (result > 0) {
            operation_reset(operation);
            return 0;
        }
    }
    operation_reset(operation);
    return -1;
}

static int run_selftest(void) {
    const char *a = "/fat/fileman-selftest-a";
    const char *b = "/fat/fileman-selftest-b";
    const char *c = "/fat/fileman-selftest-c";
    const char *dir = "/fat/fileman-selftest-dir";
    const char *sub = "/fat/fileman-selftest-dir/sub";
    const char *nested = "/fat/fileman-selftest-dir/sub/note.txt";
    const char *copy = "/fat/fileman-selftest-copy";
    const char *copy_nested = "/fat/fileman-selftest-copy/sub/note.txt";
    struct srv_stat info;
    struct fileman_operation operation;
    const char *app = 0;
    const char *label = 0;
    struct file_entry sample[3] = {
        { "z.txt", "/fat/z.txt", "9B", 9, 0 },
        { "a-dir", "/fat/a-dir", "DIR", 0, 1 },
        { "a.txt", "/fat/a.txt", "1B", 1, 0 },
    };
    sort_entries(sample, 3, FILEMAN_SORT_NAME, 0);
    if (!sample[0].dir || !cli_streq(sample[0].name, "a-dir") ||
        !cli_streq(sample[1].name, "a.txt")) {
        srv_puts("fileman-selftest: sort failed\n");
        return 1;
    }
    if (!fileman_open_app("/fat/image.BMP", &app, &label) ||
        !cli_streq(app, "/fat/bin/paint") || !cli_streq(label, "PAINT")) {
        srv_puts("fileman-selftest: bmp route failed\n");
        return 1;
    }
    if (!fileman_open_app("/fat/readme.md", &app, &label) ||
        !cli_streq(app, "/fat/bin/textedit") || !cli_streq(label, "TEXT EDIT")) {
        srv_puts("fileman-selftest: text route failed\n");
        return 1;
    }
    if (fileman_open_app("/fat/blob.bin", &app, &label)) {
        srv_puts("fileman-selftest: unsupported route failed\n");
        return 1;
    }
    (void)srv_unlink(a);
    (void)srv_unlink(b);
    (void)srv_unlink(c);
    (void)srv_unlink(copy_nested);
    (void)srv_rmdir("/fat/fileman-selftest-copy/sub");
    (void)srv_rmdir(copy);
    (void)srv_unlink(nested);
    (void)srv_rmdir(sub);
    (void)srv_rmdir(dir);
    if (srv_mkdir(dir) < 0) {
        srv_puts("fileman-selftest: mkdir failed\n");
        return 1;
    }
    if (srv_fs_write(a, "ok\n", 3) < 0) {
        srv_puts("fileman-selftest: write failed\n");
        return 1;
    }
    if (srv_mkdir(sub) < 0 || srv_fs_write(nested, "nested\n", 7) < 0) {
        srv_puts("fileman-selftest: nested setup failed\n");
        return 1;
    }
    if (copy_file(a, b) != 0) {
        srv_puts("fileman-selftest: copy failed\n");
        return 1;
    }
    if (move_tree(b, c, 0) != 0) {
        srv_puts("fileman-selftest: move failed\n");
        return 1;
    }
    if (copy_tree(dir, copy, 0) != 0 || srv_stat(copy_nested, &info) != 0) {
        srv_puts("fileman-selftest: recursive copy failed\n");
        return 1;
    }
    if (delete_tree(copy, 0) != 0 || srv_stat(copy, &info) == 0) {
        srv_puts("fileman-selftest: recursive delete failed\n");
        return 1;
    }
    operation.copy_open = 0;
    operation.copy_in_fd = -1;
    operation.copy_out_fd = -1;
    operation_reset(&operation);
    if (operation_start_copy(&operation, FILEMAN_OP_COPY, dir, copy, "copy") != 0 ||
        run_operation_to_completion(&operation) != 0 ||
        srv_stat(copy_nested, &info) != 0) {
        srv_puts("fileman-selftest: async copy failed\n");
        return 1;
    }
    if (operation_start_delete(&operation, copy, 0) != 0 ||
        run_operation_to_completion(&operation) != 0 ||
        srv_stat(copy, &info) == 0) {
        srv_puts("fileman-selftest: async delete failed\n");
        return 1;
    }
    if (srv_unlink(a) < 0 || srv_unlink(c) < 0 || delete_tree(dir, 0) != 0) {
        srv_puts("fileman-selftest: cleanup failed\n");
        return 1;
    }
    srv_puts("fileman-selftest: ok\n");
    return 0;
}

int main(int argc, char **argv) {
    struct gui2_window window;
    struct gui2_context context;
    struct gui2_list list;
    struct gui2_textbox name_box;
    struct gui2_button up;
    struct gui2_button open;
    struct gui2_button newdir;
    struct gui2_button rename;
    struct gui2_button del;
    struct gui2_button copy;
    struct gui2_button move;
    struct gui2_button refresh;
    struct gui2_dialog confirm;
    struct gui2_dialog progress;
    struct file_entry entries[ENTRY_MAX];
    struct gui2_list_item items[ENTRY_MAX];
    struct fileman_operation operation;
    char cwd[CLI_PATH_MAX];
    char name[NAME_MAX];
    char target[CLI_PATH_MAX];
    char pending_delete[CLI_PATH_MAX];
    char confirm_text[96];
    char status[80];
    uint64_t entry_count = 0;
    if (argc > 1 && cli_streq(argv[1], "--selftest")) {
        return run_selftest();
    }

    srv_puts("fileman: start\n");
    cli_normalize_path(cwd, sizeof(cwd), "/", argc > 1 ? argv[1] : "/fat");
    if (!path_is_directory(cwd)) {
        cli_copy(cwd, sizeof(cwd), "/");
    }
    cli_copy(status, sizeof(status), "READY");

    if (gui2_window_open(&window, WIN, "FILES",
            180, 160, WIDTH, HEIGHT, gui2_theme_default()->canvas) != 0) {
        srv_puts("fileman: window open failed\n");
        return 1;
    }

    gui2_context_init(&context);
    gui2_list_init(&list, 14, 76, WIDTH - 28, 260);
    gui2_list_set_columns(&list, file_columns,
        sizeof(file_columns) / sizeof(file_columns[0]));
    gui2_textbox_init(&name_box, 14, 348, WIDTH - 28, 30, name, sizeof(name));
    gui2_textbox_set_placeholder(&name_box, "NAME FOR NEW, RENAME, COPY, MOVE");
    gui2_button_init(&up, 14, 388, 88, 30, "UP");
    gui2_button_init(&open, 114, 388, 88, 30, "OPEN");
    gui2_button_init(&newdir, 214, 388, 88, 30, "NEW");
    gui2_button_init(&rename, 314, 388, 88, 30, "REN");
    gui2_button_init(&del, 414, 388, 88, 30, "DEL");
    gui2_button_init(&copy, 514, 388, 88, 30, "COPY");
    gui2_button_init(&move, 614, 388, 88, 30, "MOVE");
    gui2_button_init(&refresh, 714, 388, 88, 30, "REFRESH");
    gui2_dialog_init(&confirm, "CONFIRM", "DELETE?", "DELETE", "CANCEL");
    gui2_dialog_init(&progress, "WORKING", "PLEASE WAIT", "OK", "CANCEL");
    pending_delete[0] = '\0';
    confirm_text[0] = '\0';
    operation.copy_open = 0;
    operation.copy_in_fd = -1;
    operation.copy_out_fd = -1;
    operation_reset(&operation);
    refresh_fileman(&list, cwd, entries, items, &entry_count, status, sizeof(status));

    draw_fileman(&window, &list, &name_box, &up, &open, &newdir, &rename,
        &del, &copy, &move, &refresh, cwd, status);
    gui2_dialog_draw(&window, &confirm);
    gui2_dialog_draw(&window, &progress);
    gui2_window_present_dirty(&window);

    for (;;) {
        struct gui2_event event;
        int changed = 0;
        int closing = 0;
        struct gui2_control controls[] = {
            { GUI2_CONTROL_LIST, &list },
            { GUI2_CONTROL_TEXTBOX, &name_box },
            { GUI2_CONTROL_BUTTON, &up },
            { GUI2_CONTROL_BUTTON, &open },
            { GUI2_CONTROL_BUTTON, &newdir },
            { GUI2_CONTROL_BUTTON, &rename },
            { GUI2_CONTROL_BUTTON, &del },
            { GUI2_CONTROL_BUTTON, &copy },
            { GUI2_CONTROL_BUTTON, &move },
            { GUI2_CONTROL_BUTTON, &refresh },
        };
        while (gui2_poll_event(&window, &event) > 0) {
            if (event.type == GUI2_EVENT_CONFIGURE) {
                srv_puts("fileman: configure ");
                print_u64(event.width);
                srv_puts("x");
                print_u64(event.height);
                srv_puts("\n");
                if (gui2_window_resize(&window, event.width, event.height) != 0) {
                    srv_puts("fileman: resize failed\n");
                }
                changed = 1;
            } else if (event.type == GUI2_EVENT_FOCUS) {
                changed = 1;
            } else if (event.type == GUI2_EVENT_CLOSE) {
                srv_puts("fileman: close\n");
                closing = 1;
                break;
            }
            if (progress.active) {
                changed |= gui2_dialog_event(&progress, &event);
                if (progress.secondary_clicks != 0) {
                    progress.secondary_clicks = 0;
                    gui2_dialog_close(&progress);
                    operation_reset(&operation);
                    refresh_fileman(&list, cwd, entries, items, &entry_count,
                        status, sizeof(status));
                    cli_copy(status, sizeof(status), "OP CANCELED");
                    srv_puts("fileman: operation canceled\n");
                    changed = 1;
                }
                continue;
            }
            if (confirm.active) {
                changed |= gui2_dialog_event(&confirm, &event);
                if (confirm.primary_clicks != 0) {
                    int ok;
                    confirm.primary_clicks = 0;
                    ok = operation_start_delete(&operation, pending_delete, 0) == 0;
                    gui2_dialog_close(&confirm);
                    pending_delete[0] = '\0';
                    if (ok) {
                        operation_format_progress(&operation, status, sizeof(status));
                        update_progress_dialog(&progress, &operation, status);
                    } else {
                        cli_copy(status, sizeof(status), "DELETE FAILED");
                    }
                    srv_puts("fileman: delete confirmed\n");
                    changed = 1;
                }
                if (confirm.secondary_clicks != 0) {
                    confirm.secondary_clicks = 0;
                    gui2_dialog_close(&confirm);
                    pending_delete[0] = '\0';
                    cli_copy(status, sizeof(status), "DELETE CANCELED");
                    changed = 1;
                }
                continue;
            }
            changed |= gui2_dispatch_event(&context, &event,
                controls, sizeof(controls) / sizeof(controls[0]));
            if (operation.active) {
                up.clicks = 0;
                open.clicks = 0;
                list.activations = 0;
                newdir.clicks = 0;
                rename.clicks = 0;
                del.clicks = 0;
                copy.clicks = 0;
                move.clicks = 0;
                refresh.clicks = 0;
                continue;
            }
            if (list.header_clicks != 0) {
                char selected_name[NAME_MAX];
                list.header_clicks = 0;
                selected_name[0] = '\0';
                if (entry_count != 0 && list.selected < entry_count) {
                    cli_copy(selected_name, sizeof(selected_name), entries[list.selected].name);
                }
                if (list.sort_column == list.clicked_column) {
                    list.sort_desc = !list.sort_desc;
                } else {
                    list.sort_column = list.clicked_column;
                    list.sort_desc = 0;
                }
                refresh_fileman_select(&list, cwd, entries, items, &entry_count,
                    status, sizeof(status), selected_name);
                srv_puts("fileman: sort\n");
                changed = 1;
            }
            if (up.clicks != 0) {
                up.clicks = 0;
                parent_path(cwd, sizeof(cwd), cwd);
                refresh_fileman(&list, cwd, entries, items, &entry_count,
                    status, sizeof(status));
                srv_puts("fileman: up\n");
                changed = 1;
            }
            if (open.clicks != 0 || list.activations != 0) {
                open.clicks = 0;
                list.activations = 0;
                if (entry_count != 0 && list.selected < entry_count) {
                    if (entries[list.selected].dir) {
                        cli_copy(cwd, sizeof(cwd), entries[list.selected].path);
                        list.selected = 0;
                        refresh_fileman(&list, cwd, entries, items, &entry_count,
                            status, sizeof(status));
                        srv_puts("fileman: open dir\n");
                    } else {
                        const char *app = 0;
                        const char *label = 0;
                        if (fileman_open_app(entries[list.selected].path, &app, &label)) {
                            long pid = srv_spawn_bg_args(app, entries[list.selected].path);
                            if (pid >= 0 && label != 0) {
                                cli_copy(status, sizeof(status),
                                    cli_streq(label, "PAINT") ? "OPENED PAINT" :
                                    "OPENED TEXT EDIT");
                            } else {
                                cli_copy(status, sizeof(status), "OPEN FAILED");
                            }
                            srv_puts("fileman: open ");
                            srv_puts(label);
                            srv_puts("\n");
                        } else {
                            cli_copy(status, sizeof(status), "UNSUPPORTED TYPE");
                            srv_puts("fileman: unsupported file\n");
                        }
                    }
                }
                changed = 1;
            }
            if (newdir.clicks != 0) {
                newdir.clicks = 0;
                if (name_box.length != 0) {
                    int ok;
                    selected_path(target, sizeof(target), cwd, entries, entry_count,
                        list.selected, name_box.buffer);
                    ok = srv_mkdir(target) == 0;
                    clear_name(&name_box);
                    refresh_fileman(&list, cwd, entries, items, &entry_count,
                        status, sizeof(status));
                    cli_copy(status, sizeof(status), ok ? "DIRECTORY CREATED" : "NEW FAILED");
                    srv_puts("fileman: new dir\n");
                } else {
                    cli_copy(status, sizeof(status), "NAME REQUIRED");
                }
                changed = 1;
            }
            if (rename.clicks != 0) {
                rename.clicks = 0;
                if (entry_count != 0 && list.selected < entry_count && name_box.length != 0) {
                    char selected_name[NAME_MAX];
                    int ok;
                    cli_copy(selected_name, sizeof(selected_name), name_box.buffer);
                    selected_path(target, sizeof(target), cwd, entries, entry_count,
                        list.selected, name_box.buffer);
                    ok = srv_rename(entries[list.selected].path, target) == 0;
                    clear_name(&name_box);
                    refresh_fileman_select(&list, cwd, entries, items, &entry_count,
                        status, sizeof(status), selected_name);
                    cli_copy(status, sizeof(status), ok ? "RENAMED" : "RENAME FAILED");
                    srv_puts("fileman: rename\n");
                } else {
                    cli_copy(status, sizeof(status), "SELECT AND NAME REQUIRED");
                }
                changed = 1;
            }
            if (copy.clicks != 0) {
                copy.clicks = 0;
                if (entry_count != 0 && list.selected < entry_count && name_box.length != 0) {
                    char selected_name[NAME_MAX];
                    int ok;
                    cli_copy(selected_name, sizeof(selected_name), name_box.buffer);
                    selected_path(target, sizeof(target), cwd, entries, entry_count,
                        list.selected, name_box.buffer);
                    ok = operation_start_copy(&operation, FILEMAN_OP_COPY,
                        entries[list.selected].path, target, selected_name) == 0;
                    clear_name(&name_box);
                    if (ok) {
                        operation_format_progress(&operation, status, sizeof(status));
                        update_progress_dialog(&progress, &operation, status);
                    } else {
                        cli_copy(status, sizeof(status), "COPY FAILED");
                    }
                    srv_puts("fileman: copy\n");
                } else {
                    cli_copy(status, sizeof(status), "SELECT AND NAME REQUIRED");
                }
                changed = 1;
            }
            if (move.clicks != 0) {
                move.clicks = 0;
                if (entry_count != 0 && list.selected < entry_count && name_box.length != 0) {
                    char selected_name[NAME_MAX];
                    int ok;
                    cli_copy(selected_name, sizeof(selected_name), name_box.buffer);
                    selected_path(target, sizeof(target), cwd, entries, entry_count,
                        list.selected, name_box.buffer);
                    ok = srv_rename(entries[list.selected].path, target) == 0;
                    clear_name(&name_box);
                    if (ok) {
                        struct op_stats stats;
                        stats.files = path_is_directory(target) ? 0 : 1;
                        stats.dirs = path_is_directory(target) ? 1 : 0;
                        refresh_fileman_select(&list, cwd, entries, items, &entry_count,
                            status, sizeof(status), selected_name);
                        format_op_status(status, sizeof(status), "MOVED", &stats);
                    } else if (operation_start_copy(&operation, FILEMAN_OP_MOVE,
                            entries[list.selected].path, target, selected_name) == 0) {
                        operation_format_progress(&operation, status, sizeof(status));
                        update_progress_dialog(&progress, &operation, status);
                        ok = 1;
                    } else {
                        cli_copy(status, sizeof(status), "MOVE FAILED");
                    }
                    srv_puts("fileman: move\n");
                } else {
                    cli_copy(status, sizeof(status), "SELECT AND NAME REQUIRED");
                }
                changed = 1;
            }
            if (del.clicks != 0) {
                del.clicks = 0;
                if (entry_count != 0 && list.selected < entry_count) {
                    cli_copy(pending_delete, sizeof(pending_delete),
                        entries[list.selected].path);
                    confirm_message(confirm_text, sizeof(confirm_text),
                        entries[list.selected].name);
                    gui2_dialog_open(&confirm, "CONFIRM DELETE", confirm_text);
                    srv_puts("fileman: delete prompt\n");
                } else {
                    cli_copy(status, sizeof(status), "SELECT REQUIRED");
                }
                changed = 1;
            }
            if (refresh.clicks != 0) {
                refresh.clicks = 0;
                refresh_fileman(&list, cwd, entries, items, &entry_count,
                    status, sizeof(status));
                srv_puts("fileman: refresh\n");
                changed = 1;
            }
        }
        if (closing) {
            break;
        }
        if (operation.active) {
            int result = operation_step(&operation, status, sizeof(status));
            update_progress_dialog(&progress, &operation, status);
            changed = 1;
            if (result != 0) {
                char done_status[80];
                char selected_name[NAME_MAX];
                cli_copy(done_status, sizeof(done_status), status);
                cli_copy(selected_name, sizeof(selected_name), operation.select_name);
                refresh_fileman_select(&list, cwd, entries, items, &entry_count,
                    status, sizeof(status), result > 0 ? selected_name : 0);
                cli_copy(status, sizeof(status), done_status);
                gui2_dialog_close(&progress);
                operation_reset(&operation);
            }
        }
        if (changed) {
            draw_fileman(&window, &list, &name_box, &up, &open, &newdir,
                &rename, &del, &copy, &move, &refresh, cwd, status);
            gui2_dialog_draw(&window, &confirm);
            gui2_dialog_draw(&window, &progress);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("fileman: exited\n");
    return 0;
}
