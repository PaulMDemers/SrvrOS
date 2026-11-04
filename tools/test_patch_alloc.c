#include <stdlib.h>
#include <stddef.h>
static unsigned calls, live;
static void *test_malloc(size_t n) { if (++calls == 4) return NULL; void *p = malloc(n); if (p) live++; return p; }
static void *test_realloc(void *p,size_t n) { if (++calls == 4) return NULL; void *q = realloc(p,n); if (q && !p) live++; return q; }
static void test_free(void *p) { if (p) live--; free(p); }
#define malloc test_malloc
#define realloc test_realloc
#define free test_free
#define main patch_program_main
#include "../userspace/patch/patch.c"
#undef main
#undef malloc
#undef realloc
#undef free
#include <unistd.h>
int main(void) {
    int fds[2]; if (pipe(fds)) return 1;
    if (write(fds[1], "one\ntwo\n", 8) != 8) return 2;
    close(fds[1]);
    struct line_vec lines = {0}; int ok = read_lines_from_fd(fds[0], &lines); close(fds[0]);
    if (ok || lines.items || lines.count || lines.capacity || live) return 3;
    return 0;
}
