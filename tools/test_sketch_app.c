#define _DEFAULT_SOURCE
#define main sketch_main
#include "../userspace/desksketch/desksketch.c"
#undef main
#include <assert.h>
static int canceled,save_failed,save_starts;static char copied[16];
long srv_ticks(void) { return 7; }
long srv_getpid(void) { return 42; }
void srv_puts(const char *text) { (void)text; }
int desk_app_cancel_close(struct desk_app *w) { (void)w;canceled++;return 0; }
void desk_image_job_init(struct desk_image_job *j) { *j=(struct desk_image_job){0}; }
int desk_image_job_start(struct desk_image_job *j,const char *path,uint64_t now) { (void)path;(void)now;j->busy=1;return 0; }
void desk_image_job_cancel(struct desk_image_job *j) { j->busy=0; }
uint32_t *desk_image_job_take(struct desk_image_job *j) { uint32_t *p=j->pixels;j->pixels=NULL;return p; }
void desk_image_pixels_release(uint32_t *p,uint32_t w,uint32_t h) { if(p) munmap(p,(size_t)w*h*4); }
void desk_capture_job_init(struct desk_capture_job *j) { *j=(struct desk_capture_job){0}; }
int desk_capture_job_start(struct desk_capture_job *j,const uint32_t *pixels,uint32_t w,uint32_t h,const char *path) {
    assert(pixels==app.bitmap.pixels && w==app.bitmap.width && h==app.bitmap.height);if(save_failed) return -1;j->busy=1;strcpy(j->request.path,path);save_starts++;return 0;
}
int desk_file_job_init(struct desk_file_job *j) { *j=(struct desk_file_job){0};return 0; }
void desk_file_job_close(struct desk_file_job *j) { j->busy=0; }
int desk_file_job_start(struct desk_file_job *j,unsigned op,const char *path,const void *data,size_t len,unsigned flags,uint32_t crc) {
    (void)data;(void)len;(void)flags;(void)crc;assert(op==DESK_FILE_LIST);j->busy=1;strcpy(j->request.path,path);return 0;
}
void desk_clipboard_init(struct desk_clipboard_client *c,void *buffer) { *c=(struct desk_clipboard_client){.buffer=buffer}; }
int desk_clipboard_copy(struct desk_clipboard_client *c,uint64_t server,const void *data,size_t length,uint64_t tick) {
    (void)server;(void)tick;assert(length<sizeof(copied));memcpy(copied,data,length);copied[length]=0;c->mode=1;return 0;
}
int desk_clipboard_paste(struct desk_clipboard_client *c,uint64_t server,uint64_t tick) { (void)c;(void)server;(void)tick;return -1; }
static void dot(int x) { assert(!desk_bitmap_begin(&app.bitmap));desk_bitmap_line(&app.bitmap,x,0,x,0,0,1);assert(desk_bitmap_commit(&app.bitmap)); }
int main(void) {
    initialize();assert(!document(64,48,NULL));app.window.width=800;app.window.height=560;app.window.focused=1;layout();desk_ui_focus(&app.ui,SK_CANVAS);
    assert(app.tools.count==4 && app.paint.view.focused && app.buttons[0].enabled && !app.buttons[4].enabled);
    assert(bmp_path("/fat/a.BMP") && !bmp_path("/fat/a.png") && !bmp_path("/fat/a.bmp.old"));
    dot(0);request(SK_CLOSE);assert(app.dialog_operation==SK_CLOSE && !app.quit && !app.ui.active);
    dialog_action((struct desk_action){SK_KEEP,DESK_WIDGET_ACTIVATED});assert(!app.dialog_operation && !app.quit && canceled==1 && app.ui.active);
    request(SK_NEW);dialog_action((struct desk_action){SK_SAVE_FIRST,DESK_WIDGET_ACTIVATED});assert(app.after_save==SK_NEW && app.picker_mode==DESK_PICKER_SAVE && app.file_job.busy);
    picker_action((struct desk_picker_result){.operation=DESK_PICKER_CANCEL});assert(!app.after_save && !app.picker_mode && desk_bitmap_dirty(&app.bitmap));
    app.file_job.busy=0;save_image("/fat/first.bmp");uint64_t first=app.save_revision;assert(save_starts==1 && app.save_job.busy && app.buttons[3].enabled && app.paint.view.enabled);
    dot(1);app.save_job.busy=0;save_finish(1);assert(app.bitmap.saved_revision==first && desk_bitmap_dirty(&app.bitmap) && !strcmp(app.path,"/fat/first.bmp"));
    assert(desk_bitmap_undo(&app.bitmap) && !desk_bitmap_dirty(&app.bitmap));assert(desk_bitmap_redo(&app.bitmap));
    save_image("/fat/exists.bmp");app.save_job.busy=0;app.save_job.response.status=DESK_CAPTURE_EXISTS;save_finish(1);assert(desk_bitmap_dirty(&app.bitmap) && !strcmp(app.path,"/fat/first.bmp") && strstr(app.status,"exists"));
    app.save_job.response.status=0;save_image("/fat/latest.bmp");request(SK_CLOSE);assert(app.after_save==SK_CLOSE && !app.quit);app.save_job.busy=0;save_finish(1);assert(!desk_bitmap_dirty(&app.bitmap) && app.after_save==SK_CLOSE && !app.quit);app.after_save=0;
    save_failed=1;save_image("/fat/failure.bmp");assert(!app.save_job.busy && !app.after_save);save_failed=0;
    action((struct desk_action){SK_COLOR,DESK_WIDGET_ACTIVATED});assert(app.color_open);desk_color_panel_open(&app.colors,0x123456);
    color_action((struct desk_action){DESK_COLOR_COPY,DESK_WIDGET_ACTIVATED});assert(!strcmp(copied,"#123456"));app.clipboard.mode=0;
    color_action((struct desk_action){DESK_COLOR_CANCEL,DESK_WIDGET_ACTIVATED});assert(!app.color_open && app.paint.color==0x292e2b);
    action((struct desk_action){SK_COLOR,DESK_WIDGET_ACTIVATED});desk_color_panel_open(&app.colors,0x123456);color_action((struct desk_action){DESK_COLOR_ACCEPT,DESK_WIDGET_ACTIVATED});assert(app.paint.color==0x123456);
    void *old=app.arena;app.image_job.response.status=DESK_IMAGE_FORMAT;image_finish(1);assert(app.arena==old);
    app.image_job.response=(struct desk_image_response){.width=2,.height=1};app.image_job.pixels=mmap(NULL,8,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(app.image_job.pixels!=MAP_FAILED);app.image_job.pixels[0]=0;app.image_job.pixels[1]=0xff38644f;strcpy(app.pending_path,"/fat/source.png");image_finish(1);
    assert(app.bitmap.width==2 && app.bitmap.height==1 && app.bitmap.pixels[0]==0xffffffff && app.bitmap.pixels[1]==0xff38644f && !desk_bitmap_dirty(&app.bitmap));
    assert(!document(64,48,NULL));dot(0);app.file_job.busy=1;request(SK_CLOSE);assert(app.after_save==SK_CLOSE && !app.dialog_operation && !app.quit);app.after_save=0;app.file_job.busy=0;
    static uint32_t full[980*660],tiled[980*660];const int sizes[][2]={{800,560},{280,240},{400,320},{960,640}};
    for(unsigned mode=0;mode<8;mode++) {
        int w=sizes[mode/2][0],h=sizes[mode/2][1];app.window.width=w;app.window.height=h;app.theme.body=mode%2?&desk_font_20:&desk_font_16;layout();
        for(unsigned i=0;i<7;i++) if(app.buttons[i].frame.w) assert(app.buttons[i].frame.x>=12 && app.buttons[i].frame.x+app.buttons[i].frame.w<=w-12);
        if(mode%2) { app.color_open=1;desk_color_panel_open(&app.colors,app.paint.color);layout(); }
        memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));struct desk_canvas a,b;desk_canvas_init(&a,full,w,h,980);desk_canvas_init(&b,tiled,w,h,980);draw(&a);
        for(int y=0;y<h;y+=73) for(int x=0;x<w;x+=89) { desk_clip(&b,(struct desk_rect){x,y,89,73});draw(&b); }
        assert(!memcmp(full,tiled,sizeof(full)));app.color_open=0;
        for(unsigned y=0;y<660;y++) for(unsigned x=0;x<980;x++) if(x>=(unsigned)w || y>=(unsigned)h) assert(!full[y*980+x]);
    }
    munmap(app.arena,app.arena_bytes);
    puts("sketch-app: PASS unsaved prompts, save/cancel/deferred close, saved-revision isolation during edits, existing-file failure, color drafts/copy, import retention and compact/larger/tiled drawing");
}
