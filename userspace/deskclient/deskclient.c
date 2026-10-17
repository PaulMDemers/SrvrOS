#include <srvros/desktop.h>
#include <srvros/desk_render.h>
#include <srvros/gui.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>

static void draw(struct desk_canvas *c,const char *input,int large) {
    desk_fill(c,(struct desk_rect){0,0,c->width,c->height},0xfaf9f6);
    desk_text(c,&desk_font_20,24,24,"A little room to think.",0x292e2b);
    desk_text(c,&desk_font_16,24,68,
        "An independent application, in its own window.\n"
        "Type a few words. Drag the title. Resize the corner.\n"
        "\nThe quick brown fox jumps over the lazy dog.\n"
        "Café · naïve · £ € — Aa Bb Cc 0123456789",0x465348);
    struct desk_rect clip=c->clip;
    desk_clip(c,(struct desk_rect){20,c->height-92,c->width-40,44});
    desk_fill(c,c->clip,0xe0eade);
    desk_text(c,large?&desk_font_20:&desk_font_16,26,c->height-82,input,0x38644f);
    c->clip=clip;
    desk_text(c,&desk_font_16,24,c->height-30,"Type study   ·   Ctrl-L: text size",0x646b65);
}
int main(int argc,char **argv) {
    if (argc==2 && strcmp(argv[1],"--license")==0) { srv_puts(desk_font_license); return 0; }
    struct desk_packet open=desk_packet_new(DESK_OPEN);
    open.width=600; open.height=320;
    strcpy(open.text,"Type study");
    if (desk_send(0,&open)!=0) { srv_puts("deskclient: no compositor\n"); return 1; }
    uint64_t server=0,surface=0,serial=0,generation=0,pending=0,pending_gen=0;
    uint64_t last=srv_ticks(),frames=0;
    uint32_t width=0,height=0,*pixels=0;
    struct desk_packet configure={0};
    char input[160]={0}; size_t length=0;
    int dirty=0,large=0,failed=0,quit=0;
    while (!quit) {
        for (int n=0;n<32;n++) {
            uint64_t source; struct desk_packet p;
            int r=desk_receive(&source,&p);
            if (r<0) { failed=1; quit=1; break; }
            if (!r) break;
            if (r==2) continue;
            if (!server && p.type==DESK_CONFIGURE) server=source;
            if (!server || server!=source) continue;
            if (p.type==DESK_CLOSE) { quit=1; break; }
            if (p.type==DESK_CONFIGURE && p.generation>configure.generation &&
                p.width>=280 && p.height>=240 && p.width<=DESK_MAX_WIDTH && p.height<=DESK_MAX_HEIGHT) {
                configure=p; last=srv_ticks();
            } else if (p.type==DESK_RELEASE && p.surface==surface && p.serial==pending &&
                       p.generation==pending_gen) { pending=0; last=srv_ticks(); }
            else if (p.type==DESK_TEXT) {
                if (!dirty && !pending) last=srv_ticks();
                if (p.value==12) { large=!large; dirty=1; }
                else if ((p.value==8 || p.value==127) && length) { input[--length]=0; dirty=1; }
                else if (p.value>=32 && p.value<=126 && length+1<sizeof(input)) {
                    input[length++]=(char)p.value; input[length]=0; dirty=1;
                }
                printf("deskclient: text=%s\n",input);
            }
        }
        if (quit) break;
        if (!pending && configure.generation>generation) {
            uint32_t *next=desk_pixels_alloc(configure.width,configure.height);
            uint64_t next_surface=0;
            if (!next || gui_surface_create(configure.width,configure.height,DESK_SURFACE_STREAM,&next_surface)!=0) {
                desk_pixels_free(next,configure.width,configure.height); failed=1; break;
            }
            if (surface) gui_surface_destroy(surface);
            desk_pixels_free(pixels,width,height);
            surface=next_surface; pixels=next; width=configure.width; height=configure.height;
            generation=configure.generation; dirty=1;
        }
        if (dirty && !pending && surface) {
            struct desk_canvas c;
            desk_canvas_init(&c,pixels,width,height,width);
            draw(&c,input,large);
            struct desk_packet p=desk_packet_new(DESK_COMMIT);
            p.surface=surface; p.serial=serial+1; p.generation=generation; p.width=width; p.height=height;
            if (gui_surface_blit(surface,0,0,width,height,pixels,width)!=0) { failed=1; break; }
            if (desk_send(server,&p)==0) {
                pending=++serial; pending_gen=generation; dirty=0; last=srv_ticks(); frames++;
            }
        }
        if ((!server || pending || dirty) && (uint64_t)srv_ticks()-last>500) { failed=1; break; }
        if (server && !desk_process_alive(server)) break;
        srv_sleep_ticks(1);
    }
    if (server) { struct desk_packet p=desk_packet_new(DESK_CLOSE); desk_send(server,&p); }
    if (surface && !pending) gui_surface_destroy(surface);
    desk_pixels_free(pixels,width,height);
    printf("deskclient: %s frames=%llu input=%s\n",failed?"FAIL":"exited",(unsigned long long)frames,input);
    return failed?1:0;
}
