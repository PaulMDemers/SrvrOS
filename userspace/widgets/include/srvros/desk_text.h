#ifndef SRVROS_DESK_TEXT_H
#define SRVROS_DESK_TEXT_H
#include <stddef.h>
#include <stdint.h>

/* Bounded UTF-8 document model, independent of rendering and file I/O.
 * Supply two equally sized buffers for text and one-level undo/redo. The model
 * swaps their roles on undo; always read model.data, not the original pointer.
 * Positions are UTF-8 byte boundaries. Unsupported glyphs remain valid text.
 * Failed loads/inserts are atomic. Embedded NUL and malformed UTF-8 are rejected.
 * capacity includes NUL. Operations return 1 changed, 0 no change, -1 rejected. */
struct desk_text_buffer {
    char *data, *history_data;
    size_t capacity, length, caret, anchor;
    size_t history_length, history_caret, history_anchor;
    unsigned history; /* 0 none, 1 undo available, 2 redo available. */
    uint64_t revision;
};
int desk_text_buffer_init(struct desk_text_buffer *text,char *buffer,char *history,size_t capacity);
int desk_text_buffer_set(struct desk_text_buffer *text,const char *data,size_t length);
int desk_text_buffer_insert(struct desk_text_buffer *text,const char *data,size_t length);
int desk_text_buffer_erase(struct desk_text_buffer *text,int backwards);
int desk_text_buffer_undo(struct desk_text_buffer *text,int redo);
void desk_text_buffer_select(struct desk_text_buffer *text,size_t anchor,size_t caret);
size_t desk_text_prev(const struct desk_text_buffer *text,size_t position);
size_t desk_text_next(const struct desk_text_buffer *text,size_t position);
size_t desk_text_line_start(const struct desk_text_buffer *text,size_t position);
size_t desk_text_line_end(const struct desk_text_buffer *text,size_t position);
size_t desk_text_line_count(const struct desk_text_buffer *text);
int desk_text_utf8_valid(const char *data,size_t length);
#endif
