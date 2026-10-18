#include <srvros/desk_text.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    char a[64],b[64]; struct desk_text_buffer t;
    assert(desk_text_buffer_init(&t,a,a+1,32)==-1);
    assert(desk_text_buffer_init(&t,a,b,sizeof(a))==0);
    assert(desk_text_buffer_set(&t,"Café\nhello",11)==1);
    assert(t.length==11 && desk_text_line_count(&t)==2);
    assert(desk_text_next(&t,3)==5 && desk_text_prev(&t,5)==3);
    desk_text_buffer_select(&t,4,100); assert(t.anchor==3 && t.caret==11);
    assert(desk_text_buffer_insert(&t,"!",1)==1 && !strcmp(t.data,"Caf!"));
    assert(desk_text_buffer_undo(&t,0)==1 && !strcmp(t.data,"Café\nhello"));
    assert(t.anchor==3 && t.caret==11);
    assert(desk_text_buffer_undo(&t,0)==0);
    assert(desk_text_buffer_undo(&t,1)==1 && !strcmp(t.data,"Caf!"));
    desk_text_buffer_select(&t,3,3);
    assert(desk_text_buffer_insert(&t,"é",2)==1 && !strcmp(t.data,"Café!"));
    assert(desk_text_buffer_undo(&t,1)==0);
    assert(desk_text_buffer_erase(&t,1)==1 && !strcmp(t.data,"Caf!"));
    assert(desk_text_buffer_undo(&t,0)==1 && !strcmp(t.data,"Café!") && t.caret==5 && t.anchor==5);
    desk_text_buffer_select(&t,0,0); assert(desk_text_buffer_erase(&t,1)==0);
    assert(desk_text_buffer_erase(&t,0)==1 && !strcmp(t.data,"afé!"));
    char saved[64]; strcpy(saved,t.data); uint64_t revision=t.revision;
    assert(desk_text_buffer_insert(&t,"\xc0\xaf",2)==-1);
    assert(desk_text_buffer_insert(&t,"\xed\xa0\x80",3)==-1);
    assert(desk_text_buffer_insert(&t,"\xf4\x90\x80\x80",4)==-1);
    assert(desk_text_buffer_insert(&t,"\xe2\x82",2)==-1);
    assert(desk_text_buffer_insert(&t,"a\0b",3)==-1);
    assert(desk_text_buffer_insert(&t,t.data,1)==-1);
    assert(desk_text_buffer_set(&t,"\xff",1)==-1);
    assert(!strcmp(t.data,saved) && t.revision==revision);
    assert(desk_text_buffer_set(&t,NULL,0)==1);
    for(int i=0;i<63;i++) assert(desk_text_buffer_insert(&t,"x",1)==1);
    assert(desk_text_buffer_insert(&t,"y",1)==-1 && t.length==63 && t.data[63]==0);
    desk_text_buffer_select(&t,0,63);
    assert(desk_text_buffer_insert(&t,"🌿",4)==1 && t.length==4);
    assert(desk_text_prev(&t,4)==0 && desk_text_next(&t,0)==4);
    assert(desk_text_buffer_set(&t,"one\ntwo\n",8)==1);
    assert(desk_text_line_start(&t,6)==4 && desk_text_line_end(&t,6)==7);
    assert(desk_text_line_start(&t,8)==8 && desk_text_line_count(&t)==3);
    puts("widget-text: PASS UTF-8 boundaries, selection replacement, atomic failure, capacity, undo/redo and lines");
}
