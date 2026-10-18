#include <srvros/desk_path.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static char output[4096],input[4097];
int main(void) {
    const char *cases[][2]={{"/","/"},{"////","/"},{"/fat//home/./../home/","/fat/home"},{"/../..","/"},{"/a/b/../../c","/c"},{"/café/../résumé.txt","/résumé.txt"},{"/a/.../b","/a/.../b"}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++) { assert(!desk_path_normalize(output,sizeof(output),cases[i][0]) && !strcmp(output,cases[i][1]));strcpy(input,cases[i][0]);assert(!desk_path_normalize(input,sizeof(input),input) && !strcmp(input,cases[i][1])); }
    const char *bad[]={NULL,"","relative","/a\nb","/a\tb","/\xff","/\xc0\xaf","/\xed\xa0\x80"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) { strcpy(output,"kept");assert(desk_path_normalize(output,sizeof(output),bad[i])<0 && !strcmp(output,"kept")); }
    strcpy(output,"kept");assert(desk_path_normalize(output,4,"/abc")<0 && !strcmp(output,"kept"));assert(!desk_path_normalize(output,5,"/abc"));
    assert(!desk_path_parent(output,sizeof(output),"/fat/home/file/") && !strcmp(output,"/fat/home"));assert(!desk_path_parent(output,sizeof(output),"/") && !strcmp(output,"/"));
    assert(!desk_path_join(output,sizeof(output),"//fat/home/../","café.txt") && !strcmp(output,"/fat/café.txt"));
    assert(!desk_path_join(output,sizeof(output),"/","x") && !strcmp(output,"/x"));
    const char *bad_names[]={"",".","..","a/b","x\ny","\xff"};
    for(unsigned i=0;i<sizeof(bad_names)/sizeof(*bad_names);i++) { strcpy(output,"kept");assert(!desk_path_name_valid(bad_names[i]));assert(desk_path_join(output,sizeof(output),"/fat",bad_names[i])<0 && !strcmp(output,"kept")); }
    strcpy(output,"/fat/home");assert(!desk_path_join(output,sizeof(output),output,"file") && !strcmp(output,"/fat/home/file"));
    strcpy(output,"name");assert(!desk_path_join(output,sizeof(output),"/fat",output) && !strcmp(output,"/fat/name"));
    strcpy(output,"/a/b/c");assert(!desk_path_parent(output+1,sizeof(output)-1,output) && !strcmp(output+1,"/a/b"));
    memset(input,'a',4096);input[0]='/';input[4096]=0;strcpy(output,"kept");assert(desk_path_normalize(output,sizeof(output),input)<0 && !strcmp(output,"kept"));
    input[4095]=0;assert(!desk_path_normalize(output,sizeof(output),input) && strlen(output)==4095);
    /* Random component stacks have an independent normalized oracle. */
    uint32_t seed=7;
    for(unsigned round=0;round<10000;round++) {
        char expected[1024]="/",names[100][8];unsigned depth=0;strcpy(input,"/");
        for(unsigned part=0;part<40;part++) {
            seed=seed*1664525u+1013904223u;unsigned choice=seed%5;
            if(choice==0) { strcat(input,"../");if(depth) depth--; }
            else if(choice==1) strcat(input,"./");
            else if(choice==2) strcat(input,"//");
            else { snprintf(names[depth],8,"p%u",part);strcat(input,names[depth++]);strcat(input,"/"); }
        }
        for(unsigned i=0;i<depth;i++) { if(i) strcat(expected,"/");strcat(expected,names[i]); }
        assert(!desk_path_normalize(output,sizeof(output),input) && !strcmp(output,expected));
    }
    puts("path: PASS UTF-8/canonicalization, root clamping, join/parent, overlapping buffers, atomic bounds failures and 10000 independent component stacks");
}
