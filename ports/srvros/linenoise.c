#include "linenoise.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <srvros/conio.h>
#include <srvros/sys.h>

#define LINENOISE_DEFAULT_HISTORY_MAX 64
#define LINENOISE_MAX_LINE 512

char *linenoiseEditMore;

static char **history;
static size_t history_len;
static size_t history_max = LINENOISE_DEFAULT_HISTORY_MAX;
static linenoiseCompletionCallback *completion_callback;
static linenoiseHintsCallback *hints_callback;
static linenoiseFreeHintsCallback *free_hints_callback;

static void ln_write(const char *text) {
    srv_write(SRV_STDOUT, text, strlen(text));
}

static void ln_write_len(const char *text, size_t length) {
    srv_write(SRV_STDOUT, text, length);
}

static char *ln_strdup(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static void history_free(void) {
    for (size_t i = 0; i < history_len; i++) {
        free(history[i]);
    }
    free(history);
    history = NULL;
    history_len = 0;
}

void linenoiseFree(void *ptr) {
    free(ptr);
}

void linenoiseSetCompletionCallback(linenoiseCompletionCallback *callback) {
    completion_callback = callback;
}

void linenoiseSetHintsCallback(linenoiseHintsCallback *callback) {
    hints_callback = callback;
}

void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback *callback) {
    free_hints_callback = callback;
}

void linenoiseAddCompletion(linenoiseCompletions *completions, const char *text) {
    if (completions == NULL || text == NULL) {
        return;
    }
    char **next = realloc(completions->cvec, sizeof(char *) * (completions->len + 1));
    if (next == NULL) {
        return;
    }
    completions->cvec = next;
    completions->cvec[completions->len] = ln_strdup(text);
    if (completions->cvec[completions->len] != NULL) {
        completions->len++;
    }
}

int linenoiseHistorySetMaxLen(int len) {
    if (len < 1) {
        return 0;
    }
    history_max = (size_t)len;
    while (history_len > history_max) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char *) * (history_len - 1));
        history_len--;
    }
    return 1;
}

int linenoiseHistoryAdd(const char *line) {
    const char *cursor = line;
    if (line == NULL) {
        return 0;
    }
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor == '\0') {
        return 0;
    }
    if (history_len > 0 && strcmp(history[history_len - 1], line) == 0) {
        return 1;
    }
    if (history_max == 0) {
        return 0;
    }
    if (history_len == history_max) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char *) * (history_len - 1));
        history_len--;
    }
    char **next = realloc(history, sizeof(char *) * (history_len + 1));
    if (next == NULL) {
        return 0;
    }
    history = next;
    history[history_len] = ln_strdup(line);
    if (history[history_len] == NULL) {
        return 0;
    }
    history_len++;
    return 1;
}

int linenoiseHistorySave(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        return -1;
    }
    for (size_t i = 0; i < history_len; i++) {
        fputs(history[i], file);
        fputc('\n', file);
    }
    fclose(file);
    return 0;
}

int linenoiseHistoryLoad(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return -1;
    }
    char line[LINENOISE_MAX_LINE];
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        linenoiseHistoryAdd(line);
    }
    fclose(file);
    return 0;
}

static void refresh_line(const char *prompt,
    const char *buffer,
    size_t length,
    size_t position,
    size_t *rendered_length) {
    size_t prompt_length = strlen(prompt);
    size_t total = prompt_length + length;
    ln_write("\r");
    ln_write(prompt);
    ln_write_len(buffer, length);
    if (*rendered_length > total) {
        for (size_t i = total; i < *rendered_length; i++) {
            ln_write(" ");
        }
    }
    if (position == length) {
        *rendered_length = total;
        return;
    }
    ln_write("\r");
    ln_write(prompt);
    ln_write_len(buffer, position);
    *rendered_length = total;
}

static int read_byte(void) {
    char c = 0;
    return srv_read(SRV_STDIN, &c, 1) == 1 ? (unsigned char)c : -1;
}

static int read_byte_scan(void) {
    for (int spin = 0; spin < 8; spin++) {
        int c = kbhit();
        if (c != 0) {
            return c;
        }
        srv_yield();
    }
    for (int spin = 0; spin < 4; spin++) {
        srv_sleep_ticks(1);
        int c = kbhit();
        if (c != 0) {
            return c;
        }
    }
    return -1;
}

static void replace_buffer(char *buffer, size_t capacity, size_t *length, size_t *position, const char *text) {
    size_t source_length = strlen(text);
    if (source_length >= capacity) {
        source_length = capacity - 1;
    }
    memcpy(buffer, text, source_length);
    buffer[source_length] = '\0';
    *length = source_length;
    *position = source_length;
}

static void save_kill(char *kill, size_t kill_capacity, const char *text, size_t length) {
    if (kill_capacity == 0) {
        return;
    }
    if (length >= kill_capacity) {
        length = kill_capacity - 1;
    }
    memcpy(kill, text, length);
    kill[length] = '\0';
}

static void delete_at(char *buffer, size_t *length, size_t position) {
    if (position >= *length) {
        return;
    }
    memmove(buffer + position, buffer + position + 1, *length - position);
    (*length)--;
}

static void insert_at(char *buffer, size_t capacity, size_t *length, size_t *position, char c) {
    if (*length + 1 >= capacity) {
        return;
    }
    memmove(buffer + *position + 1, buffer + *position, *length - *position + 1);
    buffer[*position] = c;
    (*length)++;
    (*position)++;
}

static void insert_text_at(char *buffer, size_t capacity, size_t *length, size_t *position, const char *text) {
    while (*text != '\0' && *length + 1 < capacity) {
        insert_at(buffer, capacity, length, position, *text++);
    }
}

static void delete_previous_word(char *buffer,
    size_t *length,
    size_t *position,
    char *kill,
    size_t kill_capacity) {
    size_t end = *position;
    size_t start;
    if (end == 0) {
        return;
    }
    while (end > 0 && (buffer[end - 1] == ' ' || buffer[end - 1] == '\t')) {
        end--;
    }
    start = end;
    while (start > 0 && buffer[start - 1] != ' ' && buffer[start - 1] != '\t') {
        start--;
    }
    save_kill(kill, kill_capacity, buffer + start, *position - start);
    memmove(buffer + start, buffer + *position, *length - *position + 1);
    *length -= *position - start;
    *position = start;
}

static void history_prev(char *buffer,
    size_t capacity,
    size_t *length,
    size_t *position,
    size_t *history_index,
    char *draft,
    int *draft_saved) {
    if (history_len == 0 || *history_index == 0) {
        return;
    }
    if (!*draft_saved && *history_index == history_len) {
        replace_buffer(draft, capacity, length, position, buffer);
        *draft_saved = 1;
    }
    (*history_index)--;
    replace_buffer(buffer, capacity, length, position, history[*history_index]);
}

static void history_next(char *buffer,
    size_t capacity,
    size_t *length,
    size_t *position,
    size_t *history_index,
    char *draft,
    int *draft_saved) {
    if (*history_index >= history_len) {
        return;
    }
    (*history_index)++;
    if (*history_index < history_len) {
        replace_buffer(buffer, capacity, length, position, history[*history_index]);
        return;
    }
    replace_buffer(buffer, capacity, length, position, *draft_saved ? draft : "");
}

static int consume_escape(char *buffer,
    size_t capacity,
    size_t *length,
    size_t *position,
    size_t *history_index,
    char *draft,
    int *draft_saved,
    char *kill,
    size_t kill_capacity) {
    int first = read_byte_scan();
    if (first == 127 || first == '\b') {
        delete_previous_word(buffer, length, position, kill, kill_capacity);
        return 1;
    }
    if (first == 'b' || first == 'B') {
        while (*position > 0 && (buffer[*position - 1] == ' ' || buffer[*position - 1] == '\t')) {
            (*position)--;
        }
        while (*position > 0 && buffer[*position - 1] != ' ' && buffer[*position - 1] != '\t') {
            (*position)--;
        }
        return 1;
    }
    if (first == 'f' || first == 'F') {
        while (*position < *length && buffer[*position] != ' ' && buffer[*position] != '\t') {
            (*position)++;
        }
        while (*position < *length && (buffer[*position] == ' ' || buffer[*position] == '\t')) {
            (*position)++;
        }
        return 1;
    }
    if (first != '[' && first != 'O') {
        return 0;
    }
    int second = read_byte_scan();
    if (second < 0) {
        return 0;
    }
    if (second == 'A') {
        history_prev(buffer, capacity, length, position, history_index, draft, draft_saved);
        return 1;
    }
    if (second == 'B') {
        history_next(buffer, capacity, length, position, history_index, draft, draft_saved);
        return 1;
    }
    if (second == 'C') {
        if (*position < *length) {
            (*position)++;
        }
        return 1;
    }
    if (second == 'D') {
        if (*position > 0) {
            (*position)--;
        }
        return 1;
    }
    if (second == 'H') {
        *position = 0;
        return 1;
    }
    if (second == 'F') {
        *position = *length;
        return 1;
    }
    if (second == '1' || second == '7') {
        int tilde = read_byte_scan();
        if (tilde == '~') {
            *position = 0;
            return 1;
        }
    }
    if (second == '4' || second == '8') {
        int tilde = read_byte_scan();
        if (tilde == '~') {
            *position = *length;
            return 1;
        }
    }
    if (second == '3') {
        int tilde = read_byte_scan();
        if (tilde == '~') {
            delete_at(buffer, length, *position);
            return 1;
        }
    }
    return 0;
}

char *linenoise(const char *prompt) {
    char buffer[LINENOISE_MAX_LINE];
    char draft[LINENOISE_MAX_LINE];
    char kill[LINENOISE_MAX_LINE];
    struct termios original_termios;
    int raw_mode = 0;
    size_t length = 0;
    size_t position = 0;
    size_t rendered_length = 0;
    size_t history_index = history_len;
    int draft_saved = 0;
    buffer[0] = '\0';
    draft[0] = '\0';
    kill[0] = '\0';

    if (prompt == NULL) {
        prompt = "";
    }
    if (tcgetattr(SRV_STDIN, &original_termios) == 0) {
        struct termios raw = original_termios;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        raw_mode = tcsetattr(SRV_STDIN, TCSANOW, &raw) == 0;
    }
    refresh_line(prompt, buffer, length, position, &rendered_length);

    for (;;) {
        int c = read_byte();
        if (c < 0) {
            if (raw_mode) {
                tcsetattr(SRV_STDIN, TCSANOW, &original_termios);
            }
            return NULL;
        }
        if (c == '\n' || c == '\r') {
            ln_write("\n");
            buffer[length] = '\0';
            if (raw_mode) {
                tcsetattr(SRV_STDIN, TCSANOW, &original_termios);
            }
            return ln_strdup(buffer);
        }
        if (c == 1) {
            position = 0;
        } else if (c == 14) {
            history_next(buffer, sizeof(buffer), &length, &position, &history_index, draft, &draft_saved);
        } else if (c == 16) {
            history_prev(buffer, sizeof(buffer), &length, &position, &history_index, draft, &draft_saved);
        } else if (c == 3) {
            ln_write("^C\n");
            if (raw_mode) {
                tcsetattr(SRV_STDIN, TCSANOW, &original_termios);
            }
            return ln_strdup("");
        } else if (c == 5) {
            position = length;
        } else if (c == 2) {
            if (position > 0) {
                position--;
            }
        } else if (c == 6) {
            if (position < length) {
                position++;
            }
        } else if (c == 4) {
            if (length == 0) {
                if (raw_mode) {
                    tcsetattr(SRV_STDIN, TCSANOW, &original_termios);
                }
                return NULL;
            }
            delete_at(buffer, &length, position);
        } else if (c == 11) {
            save_kill(kill, sizeof(kill), buffer + position, length - position);
            buffer[position] = '\0';
            length = position;
        } else if (c == 12) {
            linenoiseClearScreen();
            rendered_length = 0;
        } else if (c == 21) {
            save_kill(kill, sizeof(kill), buffer, position);
            buffer[0] = '\0';
            length = 0;
            position = 0;
        } else if (c == 23) {
            delete_previous_word(buffer, &length, &position, kill, sizeof(kill));
        } else if (c == 25) {
            insert_text_at(buffer, sizeof(buffer), &length, &position, kill);
        } else if (c == '\b' || c == 127) {
            if (position > 0) {
                position--;
                delete_at(buffer, &length, position);
            }
        } else if (c == '\t') {
            if (completion_callback != NULL) {
                linenoiseCompletions completions = {0, NULL};
                completion_callback(buffer, &completions);
                if (completions.len == 1 && completions.cvec[0] != NULL) {
                    replace_buffer(buffer, sizeof(buffer), &length, &position, completions.cvec[0]);
                } else if (completions.len > 1) {
                    ln_write("\n");
                    for (size_t i = 0; i < completions.len; i++) {
                        if (completions.cvec[i] != NULL) {
                            ln_write(completions.cvec[i]);
                            ln_write("\n");
                        }
                    }
                    rendered_length = 0;
                }
                for (size_t i = 0; i < completions.len; i++) {
                    free(completions.cvec[i]);
                }
                free(completions.cvec);
            }
        } else if (c == 27) {
            consume_escape(buffer,
                sizeof(buffer),
                &length,
                &position,
                &history_index,
                draft,
                &draft_saved,
                kill,
                sizeof(kill));
        } else if (c >= 32 && c < 127 && position == length) {
            insert_at(buffer, sizeof(buffer), &length, &position, (char)c);
            ln_write_len(buffer + length - 1, 1);
            rendered_length++;
            continue;
        } else if (c >= 32 && c < 127) {
            insert_at(buffer, sizeof(buffer), &length, &position, (char)c);
        }
        refresh_line(prompt, buffer, length, position, &rendered_length);
    }
}

int linenoiseEditStart(struct linenoiseState *state, int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt) {
    if (state == NULL || buf == NULL || buflen == 0) {
        return -1;
    }
    state->ifd = stdin_fd;
    state->ofd = stdout_fd;
    state->buf = buf;
    state->buflen = buflen;
    state->prompt = prompt;
    state->plen = prompt != NULL ? strlen(prompt) : 0;
    state->pos = 0;
    state->len = 0;
    state->history_index = (int)history_len;
    buf[0] = '\0';
    refresh_line(prompt != NULL ? prompt : "", buf, 0, 0, &state->oldpos);
    return 0;
}

char *linenoiseEditFeed(struct linenoiseState *state) {
    (void)state;
    return NULL;
}

void linenoiseEditStop(struct linenoiseState *state) {
    (void)state;
}

void linenoiseHide(struct linenoiseState *state) {
    (void)state;
}

void linenoiseShow(struct linenoiseState *state) {
    if (state != NULL) {
        refresh_line(state->prompt != NULL ? state->prompt : "", state->buf, state->len, state->pos, &state->oldpos);
    }
}

void linenoiseClearScreen(void) {
    clrscr();
}

void linenoiseSetMultiLine(int multiline) {
    (void)multiline;
}

void linenoisePrintKeyCodes(void) {
    ln_write("linenoise: keycode printer unavailable on srvros\n");
}

void linenoiseMaskModeEnable(void) {
}

void linenoiseMaskModeDisable(void) {
}

__attribute__((destructor))
static void linenoise_cleanup(void) {
    if (free_hints_callback != NULL && hints_callback != NULL) {
        (void)free_hints_callback;
    }
    history_free();
}
