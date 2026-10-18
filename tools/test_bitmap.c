#include <srvros/desk_bitmap.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void dot(struct desk_bitmap *b,int x,int y,uint32_t color) { assert(!desk_bitmap_begin(b));desk_bitmap_line(b,x,y,x,y,color,1);assert(desk_bitmap_commit(b)); }
int main(void) {
    size_t size=desk_bitmap_storage_bytes(64,48,4);uint32_t *arena=malloc(size+32);assert(arena);
    memset((char *)arena+size,0xa5,32);struct desk_bitmap b;
    assert(!desk_bitmap_init(&b,arena,size,64,48,4,NULL) && !desk_bitmap_dirty(&b));
    assert(!desk_bitmap_begin(&b));desk_bitmap_line(&b,0,0,63,47,0x123456,1);assert(b.changed && desk_bitmap_dirty(&b));assert(!desk_bitmap_undo(&b));
    desk_bitmap_cancel(&b);assert(!desk_bitmap_dirty(&b));for(unsigned i=0;i<64*48;i++) assert(b.pixels[i]==0xffffffffu);
    assert(!desk_bitmap_begin(&b));desk_bitmap_line(&b,5,7,50,7,0xff0000,1);assert(desk_bitmap_commit(&b));
    for(unsigned y=0;y<48;y++) for(unsigned x=0;x<64;x++) assert(b.pixels[y*64+x]==(y==7 && x>=5 && x<=50?0xffff0000u:0xffffffffu));
    uint64_t saved=desk_bitmap_revision(&b);b.saved_revision=saved;assert(!desk_bitmap_dirty(&b));dot(&b,0,0,0);assert(desk_bitmap_dirty(&b));
    assert(desk_bitmap_undo(&b) && !desk_bitmap_dirty(&b));assert(desk_bitmap_redo(&b) && desk_bitmap_dirty(&b));
    assert(desk_bitmap_undo(&b));dot(&b,1,1,0);assert(!desk_bitmap_redo(&b));
    assert(!desk_bitmap_begin(&b));desk_bitmap_line(&b,1,1,1,1,0,1);assert(!desk_bitmap_commit(&b));
    uint64_t revision=desk_bitmap_revision(&b);assert(!desk_bitmap_begin(&b));desk_bitmap_line(&b,INT32_MIN,INT32_MIN,INT32_MAX,INT32_MAX,0x123456,32);desk_bitmap_cancel(&b);assert(desk_bitmap_revision(&b)==revision);
    /* Preview replaces the old transient shape; no ghost is retained. */
    assert(!desk_bitmap_begin(&b));desk_bitmap_box(&b,2,2,12,12,0x123456,1);desk_bitmap_restore(&b);desk_bitmap_box(&b,20,20,30,30,0x123456,1);assert(desk_bitmap_commit(&b));
    assert(b.pixels[2*64+2]==0xffffffffu && b.pixels[20*64+20]==0xff123456u && b.pixels[25*64+25]==0xffffffffu);
    for(unsigned i=0;i<10;i++) dot(&b,63,(int)i,i);
    assert(b.count==4 && b.index==3);
    for(unsigned i=0;i<3;i++) assert(desk_bitmap_undo(&b));
    assert(!desk_bitmap_undo(&b));
    for(unsigned i=0;i<3;i++) assert(desk_bitmap_redo(&b));
    assert(!desk_bitmap_redo(&b));
    assert(desk_bitmap_clear(&b,0xffffff) && !desk_bitmap_clear(&b,0xffffff));
    for(unsigned i=0;i<32;i++) assert(*((unsigned char *)arena+size+i)==0xa5);
    uint32_t source[]={0x00000000,0x80000000,0xff38644f,0x80ff0000};size_t small=desk_bitmap_storage_bytes(2,2,2);
    assert(!desk_bitmap_init(&b,arena,size,2,2,2,source));assert(b.pixels[0]==0xffffffffu && b.pixels[1]==0xff7f7f7fu && b.pixels[2]==0xff38644fu && b.pixels[3]==0xffff7f7fu);
    struct desk_bitmap old=b;assert(desk_bitmap_init(&b,arena,small-1,2,2,2,source)<0 && !memcmp(&b,&old,sizeof(b)));
    assert(desk_bitmap_init(&b,arena,size,2,2,2,arena+1)<0);assert(!desk_bitmap_storage_bytes(UINT32_MAX,1,4));assert(!desk_bitmap_storage_bytes(4096,4096,4));
    free(arena);puts("bitmap: PASS atomic strokes/previews, exact geometry, history eviction/branching/no-op, saved revision, alpha flattening, limits and guarded arena");
}
