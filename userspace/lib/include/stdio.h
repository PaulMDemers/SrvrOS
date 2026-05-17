#ifndef SRVROS_POSIX_STDIO_H
#define SRVROS_POSIX_STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define BUFSIZ 1024
#define FILENAME_MAX 160
#define L_tmpnam FILENAME_MAX
#define TMP_MAX 10000

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

typedef struct FILE FILE;
typedef long fpos_t;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list args);
int snprintf(char *buffer, size_t size, const char *format, ...);
int sprintf(char *buffer, const char *format, ...);
int vsnprintf(char *buffer, size_t size, const char *format, va_list args);
int vprintf(const char *format, va_list args);
int scanf(const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int sscanf(const char *text, const char *format, ...);
int vscanf(const char *format, va_list args);
int vfscanf(FILE *stream, const char *format, va_list args);
int vsscanf(const char *text, const char *format, va_list args);
int puts(const char *text);
int fputs(const char *text, FILE *stream);
int fputc(int c, FILE *stream);
#define putc(c, stream) fputc((c), (stream))
int getc(FILE *stream);
int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);
int putchar(int c);
FILE *fopen(const char *path, const char *mode);
FILE *fdopen(int fd, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int fclose(FILE *stream);
int fflush(FILE *stream);
int setvbuf(FILE *stream, char *buffer, int mode, size_t size);
void setbuf(FILE *stream, char *buffer);
int setlinebuf(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
char *fgets(char *text, int size, FILE *stream);
int ferror(FILE *stream);
int feof(FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);
long ftell(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
void rewind(FILE *stream);
int fgetpos(FILE *stream, fpos_t *position);
int fsetpos(FILE *stream, const fpos_t *position);
int remove(const char *path);
void perror(const char *prefix);
FILE *tmpfile(void);
char *tmpnam(char *buffer);
FILE *popen(const char *command, const char *mode);
int pclose(FILE *stream);

#endif
