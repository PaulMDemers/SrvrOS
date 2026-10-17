#include "scene.h"
#include <srvros/conio.h>
#include <srvros/gfx.h>
#include <srvros/gui.h>
#include <srvros/mouse.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int clamp(int64_t v,int lo,int hi) { return v<lo?lo:v>hi?hi:v; }
static struct desk_rect window_damage(struct study_scene *s) {
    struct desk_rect r=s->window; r.w+=6; r.h+=8; return r;
}
static struct desk_rect cursor_damage(struct study_scene *s) {
    return (struct desk_rect){s->cursor_x,s->cursor_y,10,17};
}

int main(int argc,char **argv) {
    if (argc==2 && strcmp(argv[1],"--license")==0) { srv_puts(desk_font_license); return 0; }
    int smoke=argc==2 && strcmp(argv[1],"--smoke")==0;
    int trace=argc==2 && strcmp(argv[1],"--trace")==0;
    struct gfx_info info={0};
    if (gfx_info(&info)!=0 || info.width<640 || info.height<480 ||
        info.width>3840 || info.height>2160) {
        srv_puts("deskstudy: unsupported framebuffer\n"); return 1;
    }
    uint32_t *pixels=malloc(info.width*info.height*sizeof(*pixels));
    struct desk_canvas canvas;
    if (!pixels || desk_canvas_init(&canvas,pixels,info.width,info.height,info.width)!=0) {
        free(pixels); return 1;
    }
    if (gui_register_server()!=0) {
        srv_puts("deskstudy: another display server is active\n"); free(pixels); return 1;
    }
    if (gfx_console_mute(1)!=0) { free(pixels); return 1; }
    struct study_scene scene={.window={48,76,640,420},.cursor_x=24,.cursor_y=56};
    scene.window.w=clamp(scene.window.w,320,canvas.width-96);
    scene.window.h=clamp(scene.window.h,300,canvas.height-136);
    struct desk_damage damage;
    desk_damage_init(&damage,canvas.width,canvas.height);
    desk_damage_add(&damage,(struct desk_rect){0,0,canvas.width,canvas.height});
    uint64_t start=srv_ticks(), frames=0, bytes=0, present_ticks=0, render_ticks=0;
    uint8_t buttons=0;
    int drag=0,resize=0,grab_x=0,grab_y=0,failed=0;
    size_t length=0;
    srv_puts("deskstudy: ready; native atlas, RAM damage, optional session\n");
    for (;;) {
        int quit=0;
        /* Bound work per turn so an input flood cannot starve presentation. */
        for (int n=0;n<32;n++) {
            /* Historical conio kbhit() returns AND consumes a character. */
            int key=kbhit();
            if (key<=0) break;
            if (key==27) { quit=1; break; }
            int changed=0;
            if (key=='\t') {
                scene.large_text=!scene.large_text;
                desk_damage_add(&damage,window_damage(&scene));
            }
            else if ((key==8 || key==127) && length) { scene.input[--length]=0; changed=1; }
            else if (key>=32 && key<=126 && length+1<sizeof(scene.input)) {
                scene.input[length++]=(char)key; scene.input[length]=0;
                changed=1;
            }
            if (changed) desk_damage_add(&damage,(struct desk_rect){scene.window.x+20,
                scene.window.y+scene.window.h-79,scene.window.w-40,38});
        }
        struct mouse_event event;
        for (int n=0;n<32 && mouse_scan(&event)>0;n++) {
            desk_damage_add(&damage,cursor_damage(&scene));
            scene.cursor_x=(int)clamp((int64_t)scene.cursor_x+event.dx,0,canvas.width-1);
            scene.cursor_y=(int)clamp((int64_t)scene.cursor_y+event.dy,0,canvas.height-1);
            if (trace) printf("deskstudy: pointer x=%d y=%d buttons=%u\n",
                scene.cursor_x,scene.cursor_y,(unsigned)event.buttons);
            struct desk_rect w=scene.window;
            int x=scene.cursor_x-w.x, y=scene.cursor_y-w.y;
            if ((event.buttons&MOUSE_LEFT) && !(buttons&MOUSE_LEFT)) {
                if (x>=w.w-40 && x<w.w && y>=0 && y<43) quit=1;
                else if (x>=w.w-24 && x<w.w && y>=w.h-24 && y<w.h) resize=1;
                else if (x>=0 && x<w.w && y>=0 && y<43) { drag=1; grab_x=x; grab_y=y; }
            }
            if (!(event.buttons&MOUSE_LEFT)) drag=resize=0;
            if (drag || resize) {
                desk_damage_add(&damage,window_damage(&scene));
                if (drag) {
                    scene.window.x=clamp(scene.cursor_x-grab_x,0,canvas.width-w.w-6);
                    scene.window.y=clamp(scene.cursor_y-grab_y,44,canvas.height-w.h-40);
                } else {
                    scene.window.w=clamp(x+1,320,canvas.width-w.x-6);
                    scene.window.h=clamp(y+1,300,canvas.height-w.y-40);
                }
                desk_damage_add(&damage,window_damage(&scene));
            }
            buttons=event.buttons;
            desk_damage_add(&damage,cursor_damage(&scene));
        }
        if (quit) break;
        for (unsigned i=0;i<damage.count;i++) {
            struct desk_rect r=damage.rects[i];
            desk_clip(&canvas,r);
            uint64_t before=srv_ticks();
            study_draw(&canvas,&scene);
            render_ticks+=(uint64_t)srv_ticks()-before;
            before=srv_ticks();
            if (blitrect(r.x,r.y,r.w,r.h,pixels+(size_t)r.y*canvas.stride+r.x,canvas.stride)!=0) failed=1;
            present_ticks+=(uint64_t)srv_ticks()-before;
            bytes+=(uint64_t)r.w*r.h*4;
        }
        if (damage.count) frames++;
        damage.count=0;
        if (failed || (smoke && (uint64_t)srv_ticks()-start>=200)) break;
        /* Current input ABI has no GUI wait primitive. Block for one tick;
         * this is temporary polling, not an event-driven compositor. */
        srv_sleep_ticks(1);
    }
    printf("deskstudy: frames=%llu writes=%llu fb_bytes=%llu render_ticks=%llu present_ticks=%llu\n",
        (unsigned long long)frames,(unsigned long long)canvas.pixel_writes,
        (unsigned long long)bytes,(unsigned long long)render_ticks,(unsigned long long)present_ticks);
    printf("deskstudy: state window=%d,%d,%d,%d large=%d input=%s\n",
        scene.window.x,scene.window.y,scene.window.w,scene.window.h,scene.large_text,scene.input);
    free(pixels);
    gfx_console_mute(0);
    srv_puts(failed?"deskstudy: FAIL\n":"deskstudy: exited\n");
    return failed?1:0;
}
