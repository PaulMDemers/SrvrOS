#include <srvros/cli.h>
#include <srvros/sys.h>

static uint64_t to_1k_blocks(uint64_t blocks, uint64_t block_size) {
    if (block_size >= 1024) {
        return blocks * (block_size / 1024);
    }
    return blocks / (1024 / block_size);
}

static void print_percent(uint64_t used, uint64_t total) {
    if (total == 0) {
        cli_puts("0%");
        return;
    }
    cli_putn((used * 100 + total - 1) / total);
    cli_puts("%");
}

static int print_df(const char *path) {
    struct srv_fsinfo info;
    if (srv_statfs(path, &info) < 0) {
        cli_puts("df: cannot stat filesystem: ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }

    uint64_t used_blocks = info.blocks > info.blocks_free ? info.blocks - info.blocks_free : 0;
    cli_puts(info.filesystem[0] != '\0' ? info.filesystem : "fs");
    cli_puts("\t");
    cli_putn(to_1k_blocks(info.blocks, info.block_size));
    cli_puts("\t");
    cli_putn(to_1k_blocks(used_blocks, info.block_size));
    cli_puts("\t");
    cli_putn(to_1k_blocks(info.blocks_available, info.block_size));
    cli_puts("\t");
    print_percent(used_blocks, info.blocks);
    cli_puts("\t");
    cli_puts(info.mountpoint[0] != '\0' ? info.mountpoint : path);
    cli_puts("\n");
    return 0;
}

int main(int argc, char **argv) {
    int status = 0;
    if (argc > 1 && cli_is_help_arg(argv[1])) {
        cli_puts("usage: df [path ...]\n");
        return 0;
    }
    cli_puts("Filesystem\t1K-blocks\tUsed\tAvailable\tUse%\tMounted on\n");
    if (argc == 1) {
        return print_df("/fat");
    }
    for (int i = 1; i < argc; i++) {
        if (print_df(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
