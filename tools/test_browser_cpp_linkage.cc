#include <semaphore.h>
#include <regex.h>
#include <stdio.h>
extern "C" int main() {
    sem_t sem;
    if (sem_init(&sem,0,0) || sem_post(&sem) || sem_wait(&sem) || sem_destroy(&sem)) return 1;
    regex_t re;
    if (regcomp(&re,"abc",REG_EXTENDED)) return 2;
    int match=regexec(&re,"abc",0,nullptr,0);
    regfree(&re);
    if (match) return 3;
    puts("browser-c-linkage: ok");
    return 0;
}
