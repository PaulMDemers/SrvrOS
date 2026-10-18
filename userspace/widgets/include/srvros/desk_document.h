#ifndef SRVROS_DESK_DOCUMENT_H
#define SRVROS_DESK_DOCUMENT_H
#include <srvros/desk_widgets.h>
#include <srvros/key_event.h>
#define DESK_DOCUMENT_BLOCKS 64
#define DESK_DOCUMENT_BYTES 32768
#define DESK_DOCUMENT_BLOCK_BYTES 4096
enum desk_document_kind { DESK_DOCUMENT_TITLE,DESK_DOCUMENT_HEADING,DESK_DOCUMENT_PARAGRAPH,DESK_DOCUMENT_CODE,DESK_DOCUMENT_NOTE };
struct desk_document_block { unsigned id;enum desk_document_kind kind;const char *text; };
/* Read-only structured text with a shared scrollbar. Block descriptors are
 * copied; text is borrowed and immutable until set again. No allocation or I/O.
 * 64 blocks, 4095 bytes per block, 32768 bytes overall. IDs are nonzero/unique.
 * Layout wraps words/long tokens and uses font metrics; content survives resize. */
struct desk_document {
    struct desk_rect frame;
    struct desk_scrollbar scroll;
    struct desk_document_block blocks[DESK_DOCUMENT_BLOCKS];
    struct { int y,height; } layout[DESK_DOCUMENT_BLOCKS];
    const struct desk_font *body;
    unsigned count;
    int enabled,focused;
};
void desk_document_init(struct desk_document *document);
int desk_document_set(struct desk_document *document,const struct desk_document_block *blocks,unsigned count);
void desk_document_configure(struct desk_document *document,struct desk_rect frame,const struct desk_font *body,int enabled);
unsigned desk_document_scroll(struct desk_document *document,int32_t delta);
unsigned desk_document_jump(struct desk_document *document,unsigned id);
unsigned desk_document_event(struct desk_document *document,struct desk_widget_event event);
unsigned desk_document_key(struct desk_document *document,const struct srv_key_event *key);
void desk_document_draw(struct desk_canvas *canvas,const struct desk_document *document,const struct desk_theme *theme);
/* Plain UTF-8 with blank lines between blocks. NULL output measures bytes
 * excluding NUL. A short destination is unchanged and returns -1. */
long desk_document_text(const struct desk_document *document,char *output,size_t capacity);
#endif
