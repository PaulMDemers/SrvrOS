#include <srvros/cli.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAKE_MAX_RULES 64
#define MAKE_MAX_DEPS 32
#define MAKE_MAX_RECIPES 32
#define MAKE_MAX_VARS 64
#define MAKE_MAX_TEXT 512
#define MAKE_MAX_PATH 160

struct variable {
    char name[64];
    char value[MAKE_MAX_TEXT];
};

struct rule {
    char target[MAKE_MAX_PATH];
    char deps[MAKE_MAX_DEPS][MAKE_MAX_PATH];
    int dep_count;
    char recipes[MAKE_MAX_RECIPES][MAKE_MAX_TEXT];
    int recipe_count;
    int phony;
    int visiting;
    int built;
};

static struct variable variables[MAKE_MAX_VARS];
static int variable_count;
static struct rule rules[MAKE_MAX_RULES];
static int rule_count;
static char default_target[MAKE_MAX_PATH];
static int dry_run;
static int always_make;

static void usage(void) {
    cli_puts("usage: make [-nB] [-f file] [target ...]\n");
}

static char *trim(char *text) {
    char *end;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    return text;
}

static void copy_text(char *out, size_t capacity, const char *text) {
    cli_copy(out, capacity, text != 0 ? text : "");
}

static void append_text(char *out, size_t capacity, const char *text) {
    size_t used = strlen(out);
    for (size_t i = 0; text != 0 && text[i] != '\0' && used + 1 < capacity; i++) {
        out[used++] = text[i];
    }
    out[used] = '\0';
}

static int find_variable(const char *name) {
    for (int i = 0; i < variable_count; i++) {
        if (cli_streq(variables[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static const char *variable_value(const char *name, const struct rule *rule) {
    static char automatic[MAKE_MAX_TEXT];
    if (cli_streq(name, "@")) {
        return rule != 0 ? rule->target : "";
    }
    if (cli_streq(name, "<")) {
        return rule != 0 && rule->dep_count > 0 ? rule->deps[0] : "";
    }
    if (cli_streq(name, "^")) {
        automatic[0] = '\0';
        if (rule != 0) {
            for (int i = 0; i < rule->dep_count; i++) {
                if (i > 0) {
                    append_text(automatic, sizeof(automatic), " ");
                }
                append_text(automatic, sizeof(automatic), rule->deps[i]);
            }
        }
        return automatic;
    }
    int index = find_variable(name);
    if (index >= 0) {
        return variables[index].value;
    }
    const char *env = getenv(name);
    return env != 0 ? env : "";
}

static void set_variable(const char *name, const char *value, int only_if_missing) {
    if (name == 0 || name[0] == '\0') {
        return;
    }
    int index = find_variable(name);
    if (index >= 0) {
        if (!only_if_missing) {
            copy_text(variables[index].value, sizeof(variables[index].value), value);
        }
        return;
    }
    if (variable_count >= MAKE_MAX_VARS) {
        return;
    }
    copy_text(variables[variable_count].name, sizeof(variables[variable_count].name), name);
    copy_text(variables[variable_count].value, sizeof(variables[variable_count].value), value);
    variable_count++;
}

static void expand_text(const char *input, const struct rule *rule, char *out, size_t capacity) {
    size_t used = 0;
    for (size_t i = 0; input != 0 && input[i] != '\0' && used + 1 < capacity; i++) {
        if (input[i] != '$') {
            out[used++] = input[i];
            continue;
        }
        i++;
        char name[64];
        size_t name_used = 0;
        if (input[i] == '\0') {
            out[used++] = '$';
            break;
        }
        if (input[i] == '$') {
            out[used++] = '$';
            continue;
        }
        if (input[i] == '@' || input[i] == '<' || input[i] == '^') {
            name[name_used++] = input[i];
        } else if (input[i] == '(' || input[i] == '{') {
            char close = input[i] == '(' ? ')' : '}';
            i++;
            while (input[i] != '\0' && input[i] != close && name_used + 1 < sizeof(name)) {
                name[name_used++] = input[i++];
            }
        } else {
            name[name_used++] = input[i];
        }
        name[name_used] = '\0';
        const char *value = variable_value(name, rule);
        for (size_t j = 0; value[j] != '\0' && used + 1 < capacity; j++) {
            out[used++] = value[j];
        }
    }
    out[used] = '\0';
}

static int find_rule(const char *target) {
    for (int i = 0; i < rule_count; i++) {
        if (cli_streq(rules[i].target, target)) {
            return i;
        }
    }
    return -1;
}

static struct rule *add_rule(const char *target) {
    int index = find_rule(target);
    if (index >= 0) {
        rules[index].dep_count = 0;
        rules[index].recipe_count = 0;
        return &rules[index];
    }
    if (rule_count >= MAKE_MAX_RULES) {
        return 0;
    }
    struct rule *rule = &rules[rule_count++];
    memset(rule, 0, sizeof(*rule));
    copy_text(rule->target, sizeof(rule->target), target);
    if (default_target[0] == '\0' && target[0] != '.') {
        copy_text(default_target, sizeof(default_target), target);
    }
    return rule;
}

static void mark_phony(char *deps) {
    char *cursor = deps;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        char *start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        char saved = *cursor;
        *cursor = '\0';
        struct rule *rule = add_rule(start);
        if (rule != 0) {
            rule->phony = 1;
        }
        *cursor = saved;
    }
}

static void add_dependencies(struct rule *rule, char *deps) {
    char expanded[MAKE_MAX_TEXT];
    expand_text(deps, 0, expanded, sizeof(expanded));
    char *cursor = expanded;
    while (*cursor != '\0' && rule->dep_count < MAKE_MAX_DEPS) {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        char *start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        char saved = *cursor;
        *cursor = '\0';
        copy_text(rule->deps[rule->dep_count++], MAKE_MAX_PATH, start);
        *cursor = saved;
    }
}

static int parse_assignment(char *line) {
    char *op = strstr(line, "?=");
    int only_if_missing = 0;
    if (op != 0) {
        only_if_missing = 1;
    } else {
        op = strstr(line, ":=");
        if (op == 0) {
            op = strchr(line, '=');
        }
    }
    if (op == 0) {
        return 0;
    }
    char *colon = strchr(line, ':');
    if (colon != 0 && colon < op && op[0] == '=') {
        return 0;
    }
    int op_length = (op[0] == '?' || op[0] == ':') ? 2 : 1;
    op[0] = '\0';
    char *name = trim(line);
    char *value = trim(op + op_length);
    char expanded[MAKE_MAX_TEXT];
    expand_text(value, 0, expanded, sizeof(expanded));
    set_variable(name, expanded, only_if_missing);
    return 1;
}

static int parse_makefile(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == 0) {
        return 0;
    }
    char line[MAKE_MAX_TEXT];
    struct rule *current = 0;
    int line_number = 0;
    while (fgets(line, sizeof(line), file) != 0) {
        line_number++;
        if (line[0] == '\t') {
            if (current != 0 && current->recipe_count < MAKE_MAX_RECIPES) {
                char *recipe = trim(line + 1);
                copy_text(current->recipes[current->recipe_count++], MAKE_MAX_TEXT, recipe);
            }
            continue;
        }
        char *hash = strchr(line, '#');
        if (hash != 0) {
            *hash = '\0';
        }
        char *clean = trim(line);
        if (clean[0] == '\0') {
            continue;
        }
        if (parse_assignment(clean)) {
            current = 0;
            continue;
        }
        char *colon = strchr(clean, ':');
        if (colon == 0) {
            if (current != 0 && current->recipe_count < MAKE_MAX_RECIPES) {
                copy_text(current->recipes[current->recipe_count++], MAKE_MAX_TEXT, clean);
                continue;
            }
            cli_puts("make: syntax error line ");
            cli_putn((uint64_t)line_number);
            cli_puts("\n");
            fclose(file);
            return -1;
        }
        *colon = '\0';
        char *target = trim(clean);
        char *deps = trim(colon + 1);
        if (cli_streq(target, ".PHONY")) {
            mark_phony(deps);
            current = 0;
            continue;
        }
        char expanded_target[MAKE_MAX_PATH];
        expand_text(target, 0, expanded_target, sizeof(expanded_target));
        current = add_rule(expanded_target);
        if (current == 0) {
            cli_puts("make: too many rules\n");
            fclose(file);
            return -1;
        }
        add_dependencies(current, deps);
    }
    fclose(file);
    return 1;
}

static int file_mtime(const char *path, time_t *mtime) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    if (mtime != 0) {
        *mtime = st.st_mtime;
    }
    return 1;
}

static int run_rule(int rule_index);

static int build_target(const char *target) {
    int rule_index = find_rule(target);
    if (rule_index >= 0) {
        return run_rule(rule_index);
    }
    if (file_mtime(target, 0)) {
        return 0;
    }
    cli_puts("make: no rule to make target '");
    cli_puts(target);
    cli_puts("'\n");
    return 1;
}

static int rule_needs_build(const struct rule *rule) {
    if (always_make || rule->phony) {
        return 1;
    }
    time_t target_time = 0;
    if (!file_mtime(rule->target, &target_time)) {
        return 1;
    }
    for (int i = 0; i < rule->dep_count; i++) {
        time_t dep_time = 0;
        if (file_mtime(rule->deps[i], &dep_time) && dep_time > target_time) {
            return 1;
        }
    }
    return 0;
}

static int run_command_in_cwd(const char *command) {
    char cwd[MAKE_MAX_PATH];
    char wrapped[MAKE_MAX_TEXT * 2];
    if (getcwd(cwd, sizeof(cwd)) == 0 || cli_streq(cwd, "/")) {
        return system(command);
    }
    size_t used = 0;
    const char *prefix = "cd ";
    const char *separator = "; ";
    for (size_t i = 0; prefix[i] != '\0' && used + 1 < sizeof(wrapped); i++) {
        wrapped[used++] = prefix[i];
    }
    for (size_t i = 0; cwd[i] != '\0' && used + 1 < sizeof(wrapped); i++) {
        wrapped[used++] = cwd[i];
    }
    for (size_t i = 0; separator[i] != '\0' && used + 1 < sizeof(wrapped); i++) {
        wrapped[used++] = separator[i];
    }
    for (size_t i = 0; command[i] != '\0' && used + 1 < sizeof(wrapped); i++) {
        wrapped[used++] = command[i];
    }
    wrapped[used] = '\0';
    return system(wrapped);
}

static int run_rule(int rule_index) {
    struct rule *rule = &rules[rule_index];
    if (rule->built) {
        return 0;
    }
    if (rule->visiting) {
        cli_puts("make: dependency cycle at ");
        cli_puts(rule->target);
        cli_puts("\n");
        return 1;
    }
    rule->visiting = 1;
    for (int i = 0; i < rule->dep_count; i++) {
        if (build_target(rule->deps[i]) != 0) {
            rule->visiting = 0;
            return 1;
        }
    }
    if (rule_needs_build(rule)) {
        if (rule->recipe_count == 0 && !rule->phony) {
            if (rule->dep_count == 0) {
                cli_puts("make: no recipe for target '");
                cli_puts(rule->target);
                cli_puts("'\n");
                rule->visiting = 0;
                return 1;
            }
            rule->built = 1;
            rule->visiting = 0;
            return 0;
        }
        for (int i = 0; i < rule->recipe_count; i++) {
            char command[MAKE_MAX_TEXT];
            const char *recipe = rule->recipes[i];
            int silent = 0;
            int ignore_errors = 0;
            while (*recipe == '@' || *recipe == '-') {
                if (*recipe == '@') {
                    silent = 1;
                } else {
                    ignore_errors = 1;
                }
                recipe++;
            }
            expand_text(recipe, rule, command, sizeof(command));
            if (!silent || dry_run) {
                cli_puts(command);
                cli_puts("\n");
            }
            if (!dry_run) {
                int status = run_command_in_cwd(command);
                if (status != 0 && !ignore_errors) {
                    cli_puts("make: command failed: ");
                    cli_puts(command);
                    cli_puts("\n");
                    rule->visiting = 0;
                    return 1;
                }
            }
        }
    } else {
        cli_puts("make: '");
        cli_puts(rule->target);
        cli_puts("' is up to date\n");
    }
    rule->built = 1;
    rule->visiting = 0;
    return 0;
}

int main(int argc, char **argv) {
    const char *makefile = "Makefile";
    int first_target = argc;
    set_variable("SHELL", "/fat/bin/sh", 0);

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (cli_is_help_arg(arg)) {
            usage();
            return 0;
        }
        if (cli_is_option_terminator(arg)) {
            first_target = i + 1;
            break;
        }
        if (cli_streq(arg, "-n") || cli_streq(arg, "--dry-run") || cli_streq(arg, "--just-print")) {
            dry_run = 1;
        } else if (cli_streq(arg, "-B") || cli_streq(arg, "--always-make")) {
            always_make = 1;
        } else if (cli_streq(arg, "-f") || cli_streq(arg, "--file") || cli_streq(arg, "--makefile")) {
            if (i + 1 >= argc) {
                usage();
                return 2;
            }
            makefile = argv[++i];
        } else if (cli_starts_with(arg, "--file=")) {
            makefile = arg + 7;
        } else if (cli_starts_with(arg, "--makefile=")) {
            makefile = arg + 11;
        } else if (arg[0] == '-' && arg[1] == 'f' && arg[2] != '\0') {
            makefile = arg + 2;
        } else if (arg[0] == '-') {
            usage();
            return 2;
        } else {
            first_target = i;
            break;
        }
    }

    int parsed = parse_makefile(makefile);
    if (parsed == 0 && cli_streq(makefile, "Makefile")) {
        parsed = parse_makefile("makefile");
    }
    if (parsed <= 0) {
        cli_puts("make: cannot read makefile\n");
        return 1;
    }

    if (first_target >= argc) {
        if (default_target[0] == '\0') {
            cli_puts("make: no targets\n");
            return 1;
        }
        return build_target(default_target);
    }
    int status = 0;
    for (int i = first_target; i < argc; i++) {
        if (build_target(argv[i]) != 0) {
            status = 1;
            break;
        }
    }
    return status;
}
