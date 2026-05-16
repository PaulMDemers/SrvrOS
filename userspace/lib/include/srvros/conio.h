#ifndef SRVROS_USER_CONIO_H
#define SRVROS_USER_CONIO_H

#include <stdint.h>

struct conio_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t columns;
    uint64_t rows;
};

int clrscr(void);
int gotoxy(uint64_t x, uint64_t y);
int conio_info(struct conio_info *info);
int kbhit(void);
int getch(void);
void cputs(const char *text);

#endif
