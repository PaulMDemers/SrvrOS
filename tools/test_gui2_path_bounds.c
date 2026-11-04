#include "../userspace/lib/src/gui2.c"
static int listed;
long srv_stat(const char *path, struct srv_stat *info) { (void)path; info->type=1; return 0; }
long srv_list(uint64_t i, char *path, size_t capacity, uint64_t *size) {
    (void)i; (void)path; (void)capacity; (void)size; listed++; return 0;
}
int main(void) {
    static struct gui2_file_dialog dialog;
    for (size_t i=0; i<sizeof(dialog.cwd)-1; i++) dialog.cwd[i]='a';
    dialog.cwd[0]='/'; dialog.cwd[sizeof(dialog.cwd)-1]=0;
    gui2_fd_refresh(&dialog);
    return listed == 0 && dialog.count == 0 ? 0 : 1;
}
