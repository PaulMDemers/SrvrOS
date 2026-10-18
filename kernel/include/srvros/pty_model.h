#ifndef SRVROS_PTY_MODEL_H
#define SRVROS_PTY_MODEL_H
#include <srvros/syscall_numbers.h>
#include <stdbool.h>
#include <stdint.h>
#define PTY_BYTES 8192
#define PTY_LINE 1024
#define PTY_RECORDS 128
/* Caller serializes access. No allocation, scheduling, file/device I/O or
 * process lifetime work occurs in this bounded line discipline. */
struct pty_ring { uint8_t data[PTY_BYTES];uint32_t head,size; };
struct pty_model {
    struct pty_ring input,output;
    uint8_t line[PTY_LINE];uint32_t line_size;
    uint16_t records[PTY_RECORDS];uint32_t record_head,record_count;
    struct srv_termios attributes;
    struct srv_winsize size;
    uint64_t foreground_group,signals;
    bool master_closed,slave_closed;
};
void pty_model_init(struct pty_model *pty);
int64_t pty_model_read(struct pty_model *pty,bool master,uint8_t *data,uint64_t length,bool nonblocking);
int64_t pty_model_write(struct pty_model *pty,bool master,const uint8_t *data,uint64_t length);
bool pty_model_write_ready(const struct pty_model *pty,bool master,uint8_t byte);
uint16_t pty_model_poll(const struct pty_model *pty,bool master,uint16_t events);
int pty_model_attributes(struct pty_model *pty,const struct srv_termios *attributes,unsigned action);
#endif
