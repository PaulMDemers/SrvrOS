#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <srvros/desktop_apps.h>
int main(void) {
    assert(!desk_application_info(0) && !desk_application_info(DESK_APP_COUNT+1));
    assert(!desk_application_by_key(NULL) && !desk_application_by_key("") && !desk_application_by_key("srvros.unknown"));
    unsigned pinned=0;
    for(unsigned id=1;id<=DESK_APP_COUNT;id++) {
        const struct desk_application_info *app=desk_application_info(id);
        assert(app->title[0] && app->role[0] && !strncmp(app->program,"/fat/bin/",9));
        assert(desk_application_by_key(app->key)==id && desk_application_services_valid(id,0));
        assert(desk_application_for_role(app->program,app->role)==id);
        assert(!desk_application_services_valid(id,~0u));
        assert(!!app->services==(id==DESK_APP_TEXT || id==DESK_APP_PREVIEW));
        if(app->dock){assert(!(pinned&(1u<<app->dock)));pinned|=1u<<app->dock;}
        for(unsigned j=1;j<id;j++)assert(strcmp(app->key,desk_application_info(j)->key));
    }
    assert(pinned==126 && desk_dock_count()==6);
    assert(!desk_application_services_valid(0,0));
    assert(!desk_application_for_role(NULL,"text") && !desk_application_for_role("/untrusted","text") && !desk_application_for_role("/fat/bin/desktools","preview"));
    const char *images[]={"/Photo.PNG","/a.b/Photo.jPeG","/Photo.jpg","/Photo.bmp"};
    for(unsigned i=0;i<sizeof(images)/sizeof(*images);i++) {
        unsigned ids[3]={0,0,99};
        assert(desk_application_for_path(images[i])==DESK_APP_PREVIEW);
        assert(desk_application_handlers(images[i],ids,3)==2 && ids[0]==DESK_APP_PREVIEW && ids[1]==DESK_APP_TEXT && ids[2]==99);
        ids[1]=77;assert(desk_application_handlers(images[i],ids,1)==2 && ids[1]==77);
        assert(desk_application_handlers(images[i],NULL,0)==2);
    }
    const char *text[]={"plain","/a.png/plain","/Photo.png.txt","/a.jpeg/notes.","/.hidden","/file.unknown"};
    for(unsigned i=0;i<sizeof(text)/sizeof(*text);i++) {
        unsigned ids[2]={0,77};assert(desk_application_handlers(text[i],ids,2)==1);
        assert(ids[0]==DESK_APP_TEXT && ids[1]==77 && desk_application_for_path(text[i])==DESK_APP_TEXT);
    }
    assert(!desk_application_for_path(NULL) && !desk_application_for_path("") && !desk_application_for_path("/folder.png/"));
    assert(!desk_application_handlers("/",NULL,8) && !desk_application_document_rank(DESK_APP_CALCULATOR,"/file.txt"));
    puts("app-registry: PASS stable keys, program/dock descriptors, matching/precedence, handler bounds and per-app service admission");
}
