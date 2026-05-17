#ifndef SRVROS_BYACC_CONFIG_H
#define SRVROS_BYACC_CONFIG_H

#define GCC_NORETURN __attribute__((noreturn))
#define GCC_PRINTF 1
#define GCC_PRINTFLIKE(fmt, arg) __attribute__((format(printf, fmt, arg)))
#define GCC_UNUSED __attribute__((unused))

#define HAVE_FCNTL_H 1
#define HAVE_GETOPT 1
#define HAVE_MKSTEMP 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_VSNPRINTF 1

#define MAXTABLE 32500
#define MIXEDCASE_FILENAMES 1
#define STDC_HEADERS 1
#define SYSTEM_NAME "srvros"
#define YYPATCH 20260126

#endif
