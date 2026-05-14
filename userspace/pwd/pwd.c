#include <stdio.h>
#include <unistd.h>

int main(void) {
    char cwd[160];
    if (getcwd(cwd, sizeof(cwd)) == 0) {
        printf("pwd: failed\n");
        return 1;
    }
    printf("%s\n", cwd);
    return 0;
}
