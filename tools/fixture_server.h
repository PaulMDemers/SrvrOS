/* Host fixtures only. Each case forks a pristine parent: retained/one-use
 * native objects are never reset, cloned inside a case, or reused by a case. */
#ifndef SRVROS_FIXTURE_SERVER_H
#define SRVROS_FIXTURE_SERVER_H
#ifndef _WIN32
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static int fixture_server(int (*run_case)(int, char **)) {
    char request[1024];
    puts("SRVROS-FIXTURE-1"); fflush(stdout);
    while (fgets(request, sizeof(request), stdin)) {
        size_t n = strlen(request);
        if (!n || request[n-1] != '\n') return 2;
        request[--n] = 0;
        if (!n) return 2;
        for (size_t i = 0; i < n; ++i)
            if (!isalnum((unsigned char)request[i]) && !strchr("_:-", request[i])) return 2;
        FILE *out = tmpfile(), *err = tmpfile();
        if (!out || !err) return 2;
        fflush(NULL);
        pid_t child = fork();
        if (child < 0) return 2;
        if (!child) {
            struct rlimit core = {0,0};
            if (setrlimit(RLIMIT_CORE, &core) || dup2(fileno(out), STDOUT_FILENO) < 0 ||
                    dup2(fileno(err), STDERR_FILENO) < 0) _exit(2);
            alarm(60);
            char argument[1100];
            if (snprintf(argument, sizeof(argument), "--wake-engine-%s", request) < 0) _exit(2);
            char *argv[] = {"fixture-case", argument, NULL};
            exit(run_case(2, argv));
        }
        int status;
        if (waitpid(child, &status, 0) != child) return 2;
        int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);
        long lengths[2]; FILE *streams[] = {out, err};
        for (unsigned i=0; i<2; ++i) {
            if (fseek(streams[i], 0, SEEK_END)) return 2;
            lengths[i] = ftell(streams[i]);
            if (lengths[i] < 0 || lengths[i] > 8*1024*1024 || fseek(streams[i], 0, SEEK_SET)) return 2;
        }
        printf("%d %ld %ld\n", rc, lengths[0], lengths[1]);
        for (unsigned i=0; i<2; ++i) {
            unsigned char bytes[4096]; size_t size;
            while ((size = fread(bytes, 1, sizeof(bytes), streams[i])))
                if (fwrite(bytes, 1, size, stdout) != size) return 2;
            if (ferror(streams[i])) return 2;
            fclose(streams[i]);
        }
        fflush(stdout);
    }
    return ferror(stdin) ? 2 : 0;
}
#endif
#endif
