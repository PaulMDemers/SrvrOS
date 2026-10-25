#define main preview_main
#include "../userspace/deskview/deskview.c"
#undef main
#include <assert.h>
#include <srvros/desktop_apps.h>
#include <sys/mman.h>
long srv_ticks(void) { return 0; }
void srv_puts(const char *text) { (void)text; }
int desk_file_job_start(struct desk_file_job *j,unsigned op,const char *path,
    const void *data,size_t length,unsigned flags,uint32_t crc) {
    assert(op==DESK_FILE_LIST && !data && !length && !flags && !crc && !j->busy);
    j->busy=1;j->request.id=++j->next_id;j->request.operation=op;strcpy(j->request.path,path);return 0;
}
static uint32_t *pixels(unsigned w,unsigned h) {
    uint32_t *p=mmap(NULL,(size_t)w*h*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);
    for(unsigned i=0;i<w*h;i++) p[i]=0xff000000u|i*37;
    return p;
}
int main(void) {
    assert(desk_dock_count()==6 && !desk_application_info(0) && !desk_application_info(DESK_APP_COUNT+1));
    assert(!desk_application_info(DESK_APP_PREVIEW)->dock && !strcmp(desk_application_info(DESK_APP_PREVIEW)->program,"/fat/bin/deskview"));
    assert(desk_application_for_path("/fat/Photo.JPEG")==DESK_APP_PREVIEW && desk_application_for_path("/fat/Photo.PNG")==DESK_APP_PREVIEW);
    assert(desk_application_for_path("/fat/Photo.bmp")==DESK_APP_PREVIEW && desk_application_for_path("/fat/Photo.png.txt")==DESK_APP_TEXT);
    assert(desk_application_for_path("/folder.png/file")==DESK_APP_TEXT && desk_application_for_path("plain")==DESK_APP_TEXT);
    struct desk_file_entry records[3]={{.name="z.txt",.inode=2,.size=12},{.name="folder",.inode=3,.directory=1},{.name="a.txt",.inode=1,.size=0}};
    assert(desk_listing_decode(app.entries,app.items,app.details,records,sizeof(records),3)==3);
    assert(app.entries[0].directory && !strcmp(app.entries[1].name,"a.txt") && !strcmp(app.details[2],"12 bytes"));
    struct desk_file_entry saved[3];memcpy(saved,app.entries,sizeof(saved));
    records[0].inode=1;assert(desk_listing_decode(app.entries,app.items,app.details,records,sizeof(records),3)<0 && !memcmp(saved,app.entries,sizeof(saved)));
    records[0].inode=2;strcpy(records[0].name,"bad/name");assert(desk_listing_decode(app.entries,app.items,app.details,records,sizeof(records),3)<0);
    strcpy(records[0].name,"bad\nname");assert(desk_listing_decode(app.entries,app.items,app.details,records,sizeof(records),3)<0);
    assert(desk_listing_decode(app.entries,app.items,app.details,NULL,0,0)==0);
    desk_ui_init(&app.ui);desk_image_init(&app.view);desk_picker_init(&app.picker);desk_image_job_init(&app.image_job);
    static const char *labels[]={"Open","Cancel","Fit","1:1","−","+"};
    for(unsigned i=0;i<6;i++) { desk_button_centered(&app.buttons[i],1);desk_ui_add(&app.ui,i+1,DESK_CONTROL_BUTTON,&app.buttons[i],labels[i]); }
    desk_ui_add(&app.ui,VIEW_IMAGE,DESK_CONTROL_IMAGE,&app.view,NULL);
    app.window.width=760;app.window.height=520;app.window.focused=1;layout();
    assert(app.buttons[0].enabled && !app.buttons[1].enabled && !app.buttons[2].enabled);
    app.image_job.busy=1;layout();assert(!app.buttons[0].enabled && app.buttons[1].enabled);
    app.image_job.busy=0;app.image_job.ready=1;app.image_job.response=(struct desk_image_response){.width=64,.height=48};
    app.image_job.pixels=pixels(64,48);strcpy(app.pending_path,"/fat/pictures/test.png");image_finish(1);
    assert(app.pixels && app.view.pixels==app.pixels && app.view.width==64 && app.view.height==48 && app.title_dirty);
    assert(!strcmp(app.path,app.pending_path) && app.buttons[2].enabled && app.view.focused);
    uint32_t *original=app.pixels;app.image_job.response.status=DESK_IMAGE_FORMAT;image_finish(1);
    assert(app.pixels==original && app.view.pixels==original && strstr(app.status,"decoded"));
    image_finish(-1);assert(app.pixels==original && strstr(app.status,"previous image"));
    app.image_job.response=(struct desk_image_response){.width=128,.height=128};app.image_job.pixels=pixels(128,128);app.image_job.ready=1;
    strcpy(app.pending_path,"/fat/second.png");image_finish(1);assert(app.pixels!=original && app.view.width==128);
    static uint32_t full[980*660],tiled[980*660];struct desk_canvas a,b;
    const int sizes[][2]={{760,520},{280,240},{400,320},{960,640}};
    for(unsigned mode=0;mode<4;mode++) {
        app.window.width=sizes[mode][0];app.window.height=sizes[mode][1];layout();
        for(unsigned i=0;i<6;i++) assert(app.buttons[i].frame.x>=12 && app.buttons[i].frame.x+app.buttons[i].frame.w<=sizes[mode][0]-12);
        assert(app.view.frame.w>=256 && app.view.frame.h>=128);
        if(mode==1) { desk_image_zoom(&app.view,1);desk_image_scroll(&app.view,15,20); }
        memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));desk_canvas_init(&a,full,sizes[mode][0],sizes[mode][1],980);desk_canvas_init(&b,tiled,sizes[mode][0],sizes[mode][1],980);
        draw(&a);
        for(int y=0;y<sizes[mode][1];y+=73) for(int x=0;x<sizes[mode][0];x+=89) { desk_clip(&b,(struct desk_rect){x,y,89,73});draw(&b); }
        assert(!memcmp(full,tiled,sizeof(full)));
        for(unsigned y=0;y<660;y++) for(unsigned x=0;x<980;x++) if(x>=(unsigned)sizes[mode][0] || y>=(unsigned)sizes[mode][1]) assert(!full[y*980+x]);
    }
    desk_chooser_init(&app.chooser,&app.picker,&app.file_job,app.entries,app.items,app.details);
    assert(!desk_chooser_open(&app.chooser,DESK_PICKER_OPEN,"/fat",""));app.picker_open=1;
    browse();assert(app.file_job.busy && app.chooser.pending);original=app.pixels;
    picker_close();assert(!app.picker_open && app.chooser.pending && app.file_job.busy);
    assert(desk_chooser_open(&app.chooser,DESK_PICKER_OPEN,"/other","")<0);
    app.file_job.busy=0;app.file_job.response=(struct desk_file_response){0};file_finish(1);
    assert(!app.chooser.pending && !app.picker_open && app.pixels==original && app.view.pixels==original);
    assert(!desk_chooser_open(&app.chooser,DESK_PICKER_OPEN,"/fat",""));app.picker_open=1;browse();
    app.file_job.busy=0;file_finish(-1);
    assert(app.picker_open && !app.chooser.pending && !app.picker.busy && app.pixels==original);
    picker_close();desk_image_pixels_release(app.pixels,app.view.width,app.view.height);
    puts("preview-app: PASS image retention, chooser cancel/drain/reopen/error, loading controls, focus, compact/full layout, tiled rendering and stride guards");
}
