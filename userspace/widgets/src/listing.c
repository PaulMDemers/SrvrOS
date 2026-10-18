#include <srvros/desk_listing.h>
#include <srvros/desk_text.h>
#include <string.h>
#include <stdio.h>
int desk_listing_decode(struct desk_file_entry *entries,struct desk_list_item *items,
    char details[][32],const void *data,size_t length,unsigned count) {
    if(!entries || !items || !details || (count && !data) || count>DESK_FILE_ENTRIES || length!=count*sizeof(*entries)) return -1;
    const unsigned char *bytes=data;
    for(unsigned i=0;i<count;i++) {
        struct desk_file_entry entry;memcpy(&entry,bytes+i*sizeof(entry),sizeof(entry));
        if(!memchr(entry.name,0,sizeof(entry.name)) || entry.reserved || entry.directory>1 || !entry.name[0] || !entry.inode ||
           strchr(entry.name,'/') || !strcmp(entry.name,".") || !strcmp(entry.name,"..") || !desk_text_utf8_valid(entry.name,strlen(entry.name))) return -1;
        for(unsigned j=0;j<i;j++) { struct desk_file_entry previous;memcpy(&previous,bytes+j*sizeof(previous),sizeof(previous));if(previous.inode==entry.inode || !strcmp(previous.name,entry.name)) return -1; }
        for(const char *p=entry.name;*p;p++) if((unsigned char)*p<32 || (unsigned char)*p==127) return -1;
    }
    if(count) memmove(entries,data,count*sizeof(*entries));
    for(unsigned i=1;i<count;i++) {
        struct desk_file_entry item=entries[i];unsigned j=i;
        while(j && (item.directory>entries[j-1].directory ||
            (item.directory==entries[j-1].directory && strcmp(item.name,entries[j-1].name)<0))) {
            entries[j]=entries[j-1];j--;
        }
        entries[j]=item;
    }
    for(unsigned i=0;i<count;i++) {
        if(entries[i].directory) strcpy(details[i],"Folder");
        else snprintf(details[i],32,"%llu bytes",(unsigned long long)entries[i].size);
        items[i]=(struct desk_list_item){entries[i].name,details[i],entries[i].inode,entries[i].directory};
    }
    return (int)count;
}
