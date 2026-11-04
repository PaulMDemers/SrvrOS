#ifndef DESKTOP_SESSION_SUPERVISOR_H
#define DESKTOP_SESSION_SUPERVISOR_H

/* Only CREATE returning NOTSUP before a child starts permits legacy fallback. */
#define DESKTOP_SESSION_NOTSUP (-1)
#define DESKTOP_SESSION_SWITCH 42
#define DESKTOP_SESSION_CONSOLE 43
#define DESKTOP_SESSION_CHILD_ARG "--session-child=SWITCH"
int desktop_session_supervise(int argc,char **argv);

#endif
