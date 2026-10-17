#include <assert.h>
#include <unistd.h>
#include "../ports/srvros/linenoise.c"
int main(void) {
    assert(linenoiseHistoryAdd("MixedCase-history"));
    assert(linenoiseHistorySave("/dev/full") == -1);
    char path[]="/tmp/srvros-history-XXXXXX";
    int fd=mkstemp(path); assert(fd>=0); close(fd);
    assert(linenoiseHistorySave(path)==0);
    linenoiseHistoryClear();
    assert(linenoiseHistoryLoad(path)==0);
    assert(linenoiseHistoryLen()==1);
    assert(!strcmp(linenoiseHistoryGet(0),"MixedCase-history"));
    unlink(path);
    puts("history-io-host: PASS (close failure, exact save/reload)");
}
