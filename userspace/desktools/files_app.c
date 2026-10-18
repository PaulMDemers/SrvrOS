#include "app.h"
#include <stdio.h>
#include <string.h>

static void list_directory(struct tool *t,const char *path) {
    char target[DESK_FILE_PATH];snprintf(target,sizeof(target),"%s",path);
    size_t n=strlen(target);while(n>1 && target[n-1]=='/') target[--n]=0;
    if(!t->worker_ready || desk_file_job_start(&t->job,DESK_FILE_LIST,target,NULL,0,0,0)) {
        tool_status(t,"Enter an absolute folder path and try again.");return;
    }
    tool_status(t,"Opening folder…");tool_layout(t);
}
void tool_files_init(struct tool *t,const char *path) {
    desk_ui_add(&t->ui,TOOL_UP,DESK_CONTROL_BUTTON,&t->buttons[0],"Up");
    desk_ui_add(&t->ui,TOOL_OPEN,DESK_CONTROL_BUTTON,&t->buttons[1],"Open");
    desk_ui_add(&t->ui,TOOL_REFRESH,DESK_CONTROL_BUTTON,&t->buttons[2],"Refresh");
    desk_ui_add(&t->ui,TOOL_PATH,DESK_CONTROL_EDIT,&t->path,NULL);
    desk_ui_add(&t->ui,TOOL_LIST,DESK_CONTROL_LIST,&t->list,NULL);
    desk_ui_add(&t->ui,TOOL_NAME,DESK_CONTROL_EDIT,&t->name,NULL);
    desk_ui_add(&t->ui,TOOL_MKDIR,DESK_CONTROL_BUTTON,&t->buttons[3],"New folder");
    desk_text_buffer_set(&t->path_text,path && path[0]?path:"/fat",strlen(path && path[0]?path:"/fat"));
    desk_text_buffer_set(&t->name_text,"New folder",10);
    list_directory(t,t->path_text.data);
}
static int child_path(struct tool *t,const char *name,char path[DESK_FILE_PATH]) {
    if(!name[0] || !strcmp(name,".") || !strcmp(name,"..") || strchr(name,'/')) return -1;
    int n=snprintf(path,DESK_FILE_PATH,"%s%s%s",t->saved_path,!strcmp(t->saved_path,"/")?"":"/",name);
    return n<0 || n>=DESK_FILE_PATH?-1:0;
}
void tool_files_action(struct tool *t,struct desk_action a) {
    if(t->job.busy) return;
    if(a.id==TOOL_PATH && (a.flags&DESK_EDIT_SUBMIT)) { list_directory(t,t->path_text.data);return; }
    if(!(a.flags&DESK_WIDGET_ACTIVATED) && !(a.id==TOOL_NAME && (a.flags&DESK_EDIT_SUBMIT))) return;
    if(a.id==TOOL_UP) {
        char path[DESK_FILE_PATH];strcpy(path,t->saved_path[0]?t->saved_path:"/");
        char *slash=strrchr(path,'/');if(slash && slash!=path) *slash=0;else strcpy(path,"/");
        list_directory(t,path);
    } else if(a.id==TOOL_REFRESH) list_directory(t,t->saved_path[0]?t->saved_path:t->path_text.data);
    else if(a.id==TOOL_OPEN || a.id==TOOL_LIST) {
        if(t->list.selected<0) { tool_status(t,"Select a file or folder first.");return; }
        struct desk_file_entry *entry=&t->entries[t->list.selected];char path[DESK_FILE_PATH];
        if(child_path(t,entry->name,path)) { tool_status(t,"This path is too long to open.");return; }
        if(entry->directory) list_directory(t,path);else tool_launch(t,DESK_APP_TEXT,path);
    } else if(a.id==TOOL_MKDIR || a.id==TOOL_NAME) {
        char path[DESK_FILE_PATH];
        if(child_path(t,t->name_text.data,path) || desk_file_job_start(&t->job,DESK_FILE_MKDIR,path,NULL,0,0,0)) {
            tool_status(t,"Choose a folder name without a slash.");return;
        }
        tool_status(t,"Creating folder…");tool_layout(t);
    }
}
int tool_listing_decode(struct tool *t) {
    unsigned count=t->job.response.count;
    if(count>DESK_FILE_ENTRIES || t->job.response.length!=count*sizeof(t->entries[0])) return -1;
    for(unsigned i=0;i<count;i++) {
        struct desk_file_entry entry;memcpy(&entry,t->job.incoming+i*sizeof(entry),sizeof(entry));
        if(!memchr(entry.name,0,sizeof(entry.name)) || entry.reserved || entry.directory>1 || !entry.name[0] ||
           strchr(entry.name,'/') || !strcmp(entry.name,".") || !strcmp(entry.name,"..") || !desk_text_utf8_valid(entry.name,strlen(entry.name))) return -1;
        for(const char *p=entry.name;*p;p++) if((unsigned char)*p<32 || (unsigned char)*p==127) return -1;
    }
    memcpy(t->entries,t->job.incoming,count*sizeof(t->entries[0]));
    for(unsigned i=1;i<count;i++) {
        struct desk_file_entry item=t->entries[i];unsigned j=i;
        while(j && (item.directory>t->entries[j-1].directory ||
            (item.directory==t->entries[j-1].directory && strcmp(item.name,t->entries[j-1].name)<0))) {
            t->entries[j]=t->entries[j-1];j--;
        }
        t->entries[j]=item;
    }
    for(unsigned i=0;i<count;i++) {
        if(t->entries[i].directory) strcpy(t->details[i],"Folder");
        else snprintf(t->details[i],sizeof(t->details[i]),"%llu bytes",(unsigned long long)t->entries[i].size);
        t->items[i]=(struct desk_list_item){t->entries[i].name,t->details[i],i,t->entries[i].directory};
    }
    return count;
}
void tool_files_finish(struct tool *t,int result) {
    if(result<0 || t->job.response.status) {
        tool_status(t,result<0?"The file worker stopped. Close and reopen Files to retry.":"Could not open or change this folder. Check the path and volume.");
        if(t->saved_path[0]) desk_text_buffer_set(&t->path_text,t->saved_path,strlen(t->saved_path));
        tool_layout(t);return;
    }
    if(t->job.request.operation==DESK_FILE_MKDIR) { list_directory(t,t->saved_path);return; }
    int count=tool_listing_decode(t);
    if(count<0) { tool_status(t,"The folder listing was invalid.");desk_list_set_items(&t->list,NULL,0);tool_layout(t);return; }
    strcpy(t->saved_path,t->job.request.path);desk_text_buffer_set(&t->path_text,t->saved_path,strlen(t->saved_path));
    desk_list_set_items(&t->list,t->items,count);tool_layout(t);desk_ui_focus(&t->ui,TOOL_LIST);
    snprintf(t->status,sizeof(t->status),"%u items%s",(unsigned)count,t->job.response.truncated?" · Listing limited to supported paths and 256 items":"");
    t->window.dirty=1;
    if(t->closing) t->quit=1;
}
