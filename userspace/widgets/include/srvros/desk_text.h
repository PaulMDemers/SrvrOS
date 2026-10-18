#ifndef SRVROS_DESK_TEXT_H
#define SRVROS_DESK_TEXT_H
#include <stddef.h>
#include <stdint.h>

#define DESK_TEXT_HISTORY_LIMIT 256
/* Optional caller-owned edit journal. Payload stores deleted/inserted UTF-8;
 * old records are evicted at the fixed record or byte limit. Treat fields as
 * private. Keep the journal and payload alive until the buffer is discarded. */
struct desk_text_change {
    size_t offset, position, removed, inserted, caret, anchor;
};
struct desk_text_history {
    char *data;
    size_t capacity, used;
    unsigned count, applied;
    struct desk_text_change changes[DESK_TEXT_HISTORY_LIMIT];
};
/* Bounded UTF-8 document model, independent of rendering and file I/O.
 * Supply two equally sized buffers for text and default one-level undo/redo.
 * Attach a journal for bounded multi-step history. Without a journal the model
 * swaps their roles on undo; always read model.data, not the original pointer.
 * Positions are UTF-8 byte boundaries. Unsupported glyphs remain valid text.
 * Failed loads/inserts are atomic. Embedded NUL and malformed UTF-8 are rejected.
 * capacity includes NUL. Operations return 1 changed, 0 no change, -1 rejected. */
struct desk_text_buffer {
    char *data, *history_data;
    size_t capacity, length, caret, anchor;
    size_t history_length, history_caret, history_anchor;
    unsigned history; /* Availability bits: 1 undo, 2 redo. */
    struct desk_text_history *journal;
    uint64_t revision;
};
int desk_text_buffer_init(struct desk_text_buffer *text,char *buffer,char *history,size_t capacity);
/* Attach an empty multi-step journal; existing one-level history is cleared.
 * Payload capacity must be at least twice (text capacity - 1), so every valid
 * edit is undoable. All backing stores must be disjoint. No allocation occurs. */
int desk_text_buffer_history(struct desk_text_buffer *text,struct desk_text_history *journal,
    char *payload,size_t capacity);
int desk_text_buffer_can_undo(const struct desk_text_buffer *text,int redo);
int desk_text_buffer_set(struct desk_text_buffer *text,const char *data,size_t length);
int desk_text_buffer_insert(struct desk_text_buffer *text,const char *data,size_t length);
/* Replace an explicit UTF-8 range while retaining the original selection for
 * Undo. A byte-identical replacement creates no history/revision change. */
int desk_text_buffer_replace_range(struct desk_text_buffer *text,size_t start,size_t end,
    const char *data,size_t length);
int desk_text_buffer_erase(struct desk_text_buffer *text,int backwards);
int desk_text_buffer_undo(struct desk_text_buffer *text,int redo);
void desk_text_buffer_select(struct desk_text_buffer *text,size_t anchor,size_t caret);
size_t desk_text_prev(const struct desk_text_buffer *text,size_t position);
size_t desk_text_next(const struct desk_text_buffer *text,size_t position);
size_t desk_text_line_start(const struct desk_text_buffer *text,size_t position);
size_t desk_text_line_end(const struct desk_text_buffer *text,size_t position);
size_t desk_text_line_count(const struct desk_text_buffer *text);
int desk_text_utf8_valid(const char *data,size_t length);
struct desk_text_match { size_t start,end;int wrapped; };
/* One circular search from a byte position. Backwards chooses a match strictly
 * before from; forward includes from. ASCII-only case folding; other UTF-8 bytes
 * compare exactly. Returns 1 found, 0 absent/empty query, -1 invalid input. */
int desk_text_find(const struct desk_text_buffer *text,const char *query,size_t length,
    size_t from,int backwards,int match_case,struct desk_text_match *match);
/* Replace the selected exact match, or all nonoverlapping matches in the original
 * document. Literal text only; the same ASCII case folding as find applies.
 * scratch must be disjoint from model/history/query/replacement and include room
 * for a terminator. Failure keeps document, selection and history unchanged;
 * success is one undo step. matches counts occurrences, including identical ones.
 * Returns 1 changed, 0 no change/no match, -1 invalid input or insufficient room. */
int desk_text_replace(struct desk_text_buffer *text,const char *query,size_t query_length,
    const char *replacement,size_t replacement_length,int match_case,int all,
    char *scratch,size_t scratch_capacity,size_t *matches);
#endif
