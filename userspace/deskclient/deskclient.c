#include <srvros/desktop.h>
#include <srvros/desk_render.h>
#include <srvros/gui.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>

#define STUDY_LINES 48
static int scroll_limit(int height) {
    int viewport=height-202;
    int max=STUDY_LINES*24-(viewport>0?viewport:1);
    return max>0?max:0;
}
static int scroll_clamp(int64_t n,int height) {
    int max=scroll_limit(height);
    return n<0?0:n>max?max:(int)n;
}
struct scrollbar { struct desk_rect track,thumb; int limit,travel; };
struct scrollbar_drag { int active,grab; };
static struct scrollbar scrollbar_geometry(int width,int height,int offset) {
    struct scrollbar b={.track={width-30,94,18,height-202},.limit=scroll_limit(height)};
    int thumb=b.track.h*b.track.h/(STUDY_LINES*24);
    if (thumb<16) thumb=16;
    if (thumb>b.track.h) thumb=b.track.h;
    b.travel=b.track.h-thumb;
    int top=b.track.y+(b.limit?(int)((int64_t)scroll_clamp(offset,height)*b.travel/b.limit):0);
    b.thumb=(struct desk_rect){b.track.x+4,top,10,thumb};
    return b;
}
static void scrollbar_pointer(struct scrollbar_drag *d,int width,int height,int *offset,
                              const struct desk_packet *p) {
    if (p->value&DESK_POINTER_CANCEL) { d->active=0; return; }
    int released=!(p->value&DESK_POINTER_LEFT);
    if (released && !d->active) return;
    struct scrollbar b=scrollbar_geometry(width,height,*offset);
    if (p->value&DESK_POINTER_PRESS) {
        d->active=0;
        if (p->x<b.track.x || p->x>=b.track.x+b.track.w ||
            p->y<b.track.y || p->y>=b.track.y+b.track.h) return;
        if (p->y>=b.thumb.y && p->y<b.thumb.y+b.thumb.h) {
            d->active=1; d->grab=p->y-b.thumb.y;
        } else *offset=scroll_clamp((int64_t)*offset+(p->y<b.thumb.y?-b.track.h:b.track.h),height);
        return;
    }
    if (d->active && b.travel>0) {
        int64_t top=(int64_t)p->y-d->grab-b.track.y;
        if (top<0) top=0;
        if (top>b.travel) top=b.travel;
        *offset=(int)((top*b.limit+b.travel/2)/b.travel);
    }
    if (released) d->active=0;
}
static void draw(struct desk_canvas *c,const char *input,int large,int offset) {
    desk_fill(c,(struct desk_rect){0,0,c->width,c->height},0xfaf9f6);
    desk_text(c,&desk_font_20,24,24,"A little room to think.",0x292e2b);
    desk_text(c,&desk_font_16,24,58,"Two fingers to scroll. Type below. Drag the title.",0x465348);
    struct desk_rect clip=c->clip;
    struct desk_rect viewport={24,94,c->width-58,c->height-202};
    desk_clip(c,viewport);
    static const char *lines[]={
        "A quiet space for a few thoughts.",
        "The quick brown fox jumps over the lazy dog.",
        "Café · naïve · £ € — Aa Bb Cc 0123456789",
        "Lift your fingers, then settle them again.",
        "Move the page without moving the pointer.",
        "Small movements deserve a steady response."};
    for (int i=0;i<STUDY_LINES;i++) {
        int y=viewport.y+i*24-offset;
        if (y+24<viewport.y || y>=viewport.y+viewport.h) continue;
        char text[120]; snprintf(text,sizeof(text),"%02d   %s",i+1,lines[i%6]);
        desk_text(c,&desk_font_16,viewport.x,y,text,0x465348);
    }
    c->clip=clip;
    struct scrollbar bar=scrollbar_geometry(c->width,c->height,offset);
    desk_fill(c,bar.track,0xe7ebe2);
    desk_fill(c,bar.thumb,0x748b79);
    desk_clip(c,(struct desk_rect){20,c->height-92,c->width-40,44});
    desk_fill(c,c->clip,0xe0eade);
    desk_text(c,large?&desk_font_20:&desk_font_16,26,c->height-82,input,0x38644f);
    c->clip=clip;
    char status[96]; snprintf(status,sizeof(status),"Scroll %d / %d   ·   Ctrl-L: text size",offset,scroll_limit(c->height));
    desk_text(c,&desk_font_16,24,c->height-30,status,0x646b65);
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
    int dirty=0,large=0,failed=0,quit=0,offset=0;
    struct scrollbar_drag scrollbar={0};
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
                configure=p; scrollbar.active=0; last=srv_ticks();
            } else if (p.type==DESK_RELEASE && p.surface==surface && p.serial==pending &&
                       p.generation==pending_gen) { pending=0; last=srv_ticks(); }
            else if (p.type==DESK_POINTER && height && p.generation==generation &&
                     configure.generation==generation) {
                int before=offset;
                scrollbar_pointer(&scrollbar,width,height,&offset,&p);
                if (offset!=before) {
                    if (!dirty && !pending) last=srv_ticks();
                    dirty=1; printf("deskclient: scroll=%d\n",offset);
                }
            }
            else if (p.type==DESK_SCROLL && height) {
                if (!dirty && !pending) last=srv_ticks();
                offset=scroll_clamp((int64_t)offset+p.y,height); dirty=1;
                printf("deskclient: scroll=%d\n",offset);
            }
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
            generation=configure.generation; offset=scroll_clamp(offset,height); dirty=1;
        }
        if (dirty && !pending && surface) {
            struct desk_canvas c;
            desk_canvas_init(&c,pixels,width,height,width);
            draw(&c,input,large,offset);
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
