#ifndef SRVROS_SIGNAL_SYSCALL_NUMBERS_H
#define SRVROS_SIGNAL_SYSCALL_NUMBERS_H
/* PR334 allocation, coordinated with the shared runtime owner. Assembly-safe.
 * Assignment alone does not advertise or enable a public signal capability. */
#define SYS_SIGNAL_CONTROL 164
#define SYS_SIGNAL_RETURN 165
#define SYS_SIGNAL_SUSPEND 166
#define SYS_SIGNAL_IO 167
#endif
