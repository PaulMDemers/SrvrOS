#ifndef SRVROS_DESK_TERMINAL_H
#define SRVROS_DESK_TERMINAL_H
#include <stddef.h>
#include <stdint.h>

#define DESK_TERM_COLUMNS 160
#define DESK_TERM_ROWS 64
#define DESK_TERM_REPLY 256
#define DESK_TERM_BOLD 1u
#define DESK_TERM_UNDERLINE 2u
#define DESK_TERM_REVERSE 4u
struct desk_term_cell { uint32_t codepoint,foreground,background,style; };
struct desk_term_line { struct desk_term_cell cells[DESK_TERM_COLUMNS];unsigned wrapped; };
/* Caller supplies primary scrollback (at least DESK_TERM_ROWS lines) and an
 * alternate screen of DESK_TERM_ROWS lines. No allocation, I/O or callbacks.
 * Fixed-width codepoints, bounded ANSI parser; not a complete xterm emulator. */
struct desk_terminal {
    struct desk_term_line *lines,*alternate;
    unsigned capacity,head,history,columns,rows,x,y,top,bottom;
    unsigned main_x,main_y,saved_x,saved_y;
    uint32_t main_foreground,main_background,main_style;
    uint32_t foreground,background,style,saved_foreground,saved_background,saved_style;
    unsigned alt_active,wrap,wrap_pending,origin,insert,cursor_visible,application_cursor,bracketed_paste;
    unsigned state,private_mode,param_count,param_overflow,params[16],charset,charset_target;
    unsigned utf_remaining;uint32_t utf_value,utf_minimum;
    uint8_t tabs[DESK_TERM_COLUMNS],reply[DESK_TERM_REPLY];unsigned reply_size;
    uint64_t revision,discarded,bells;
};
int desk_terminal_init(struct desk_terminal *terminal,struct desk_term_line *lines,unsigned capacity,
    struct desk_term_line *alternate,unsigned columns,unsigned rows);
int desk_terminal_resize(struct desk_terminal *terminal,unsigned columns,unsigned rows);
void desk_terminal_reset(struct desk_terminal *terminal);
void desk_terminal_feed(struct desk_terminal *terminal,const void *bytes,size_t length);
/* Logical line 0 is the oldest retained line; history is the screen's first.
 * In alternate-screen mode history is hidden and logical rows start at zero. */
const struct desk_term_line *desk_terminal_line(const struct desk_terminal *terminal,unsigned row);
unsigned desk_terminal_history(const struct desk_terminal *terminal);
size_t desk_terminal_reply(struct desk_terminal *terminal,void *bytes,size_t capacity);
/* Flatten unbracketed clipboard line breaks in place; never submit input. */
size_t desk_terminal_paste_normalize(const struct desk_terminal *terminal,char *text,size_t size);
#endif
