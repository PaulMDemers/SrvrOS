#include <srvros/desk_path.h>
#include <srvros/desk_text.h>
#include <string.h>
static int input_length(const char *s,size_t *length) {
    if(!s) return -1;
    size_t n=0;
    for(;n<DESK_PATH_INPUT_LIMIT && s[n];n++) if((unsigned char)s[n]<32 || (unsigned char)s[n]==127) return -1;
    if(n==DESK_PATH_INPUT_LIMIT || !desk_text_utf8_valid(s,n)) return -1;
    *length=n;return 0;
}
static int normalized(char result[DESK_PATH_INPUT_LIMIT],const char *input,size_t *length) {
    size_t n;if(input_length(input,&n) || !n || input[0]!='/') return -1;
    size_t used=1;result[0]='/';result[1]=0;
    for(const char *p=input;*p;) {
        while(*p=='/') p++;
        const char *start=p;while(*p && *p!='/') p++;
        size_t count=p-start;if(!count || (count==1 && start[0]=='.')) continue;
        if(count==2 && start[0]=='.' && start[1]=='.') {
            while(used>1 && result[used-1]!='/') used--;
            if(used>1) used--;
        } else {
            if(used>1) result[used++]='/';
            memcpy(result+used,start,count);used+=count;
        }
        result[used]=0;
    }
    *length=used;return 0;
}
int desk_path_normalize(char *output,size_t capacity,const char *input) {
    char result[DESK_PATH_INPUT_LIMIT];size_t n;
    if(!output || !capacity || normalized(result,input,&n) || n>=capacity) return -1;
    memmove(output,result,n+1);return 0;
}
int desk_path_name_valid(const char *name) {
    size_t n;return !input_length(name,&n) && n && !strchr(name,'/') && strcmp(name,".") && strcmp(name,"..");
}
int desk_path_join(char *output,size_t capacity,const char *directory,const char *name) {
    char result[DESK_PATH_INPUT_LIMIT];size_t n;
    if(!output || !capacity || !desk_path_name_valid(name) || normalized(result,directory,&n)) return -1;
    size_t length=strlen(name),slash=n>1;
    if(n+slash+length>=sizeof(result) || n+slash+length>=capacity) return -1;
    if(slash) result[n++]='/';
    memcpy(result+n,name,length+1);memmove(output,result,n+length+1);return 0;
}
int desk_path_parent(char *output,size_t capacity,const char *input) {
    char result[DESK_PATH_INPUT_LIMIT];size_t n;
    if(!output || !capacity || normalized(result,input,&n)) return -1;
    while(n>1 && result[n-1]!='/') n--;
    if(n>1) n--;
    if(n>=capacity) return -1;
    result[n]=0;memmove(output,result,n+1);return 0;
}
