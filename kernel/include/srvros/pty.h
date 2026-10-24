#ifndef SRVROS_PTY_H
#define SRVROS_PTY_H
#include <srvros/pty_model.h>
/* Generation-tagged endpoints; a new pair starts with one reference per end. */
uint64_t pty_create(void);
bool pty_ref(uint64_t handle,bool master);
int64_t pty_get_flags(uint64_t handle,bool master);
int64_t pty_set_flags(uint64_t handle,bool master,uint64_t flags);
void pty_close(uint64_t handle,bool master);
struct pty_model *pty_state(uint64_t handle);
int64_t pty_read(uint64_t handle,bool master,uint8_t *data,uint64_t length,bool nonblocking);
int64_t pty_write(uint64_t handle,bool master,const uint8_t *data,uint64_t length,bool nonblocking);
uint16_t pty_poll(uint64_t handle,bool master,uint16_t events);
int64_t pty_setattr(uint64_t handle,const struct srv_termios *attributes,unsigned action);
void pty_wake(void);
#endif
