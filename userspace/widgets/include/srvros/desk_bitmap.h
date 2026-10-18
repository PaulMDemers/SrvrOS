#ifndef SRVROS_DESK_BITMAP_H
#define SRVROS_DESK_BITMAP_H
#include <stddef.h>
#include <stdint.h>
#define DESK_BITMAP_HISTORY 8
#define DESK_BITMAP_MAX_PIXELS (4u*1024u*1024u)
/* Caller-owned pixel arena: one working image plus 2..8 history states. No
 * allocation or I/O. Each completed stroke/shape/clear is one undo operation.
 * Cancel restores the last committed image. New edits discard redo. */
struct desk_bitmap {
    uint32_t *pixels,*frames[DESK_BITMAP_HISTORY];
    uint32_t width,height;
    unsigned capacity,count,index,active,changed;
    uint64_t revisions[DESK_BITMAP_HISTORY],next_revision,saved_revision;
};
size_t desk_bitmap_storage_bytes(uint32_t width,uint32_t height,unsigned history);
/* Source may be NULL (white) or an opaque/straight-alpha image, flattened onto
 * white. It must be disjoint from the arena, or exactly its working pixel base. */
int desk_bitmap_init(struct desk_bitmap *bitmap,void *storage,size_t bytes,uint32_t width,uint32_t height,unsigned history,const uint32_t *source);
uint64_t desk_bitmap_revision(const struct desk_bitmap *bitmap);
int desk_bitmap_dirty(const struct desk_bitmap *bitmap);
int desk_bitmap_begin(struct desk_bitmap *bitmap);
void desk_bitmap_restore(struct desk_bitmap *bitmap);
void desk_bitmap_cancel(struct desk_bitmap *bitmap);
int desk_bitmap_commit(struct desk_bitmap *bitmap);
int desk_bitmap_undo(struct desk_bitmap *bitmap);
int desk_bitmap_redo(struct desk_bitmap *bitmap);
void desk_bitmap_line(struct desk_bitmap *bitmap,int x0,int y0,int x1,int y1,uint32_t rgb,unsigned size);
void desk_bitmap_box(struct desk_bitmap *bitmap,int x0,int y0,int x1,int y1,uint32_t rgb,unsigned size);
int desk_bitmap_clear(struct desk_bitmap *bitmap,uint32_t rgb);
#endif
