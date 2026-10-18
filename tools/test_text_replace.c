#include <srvros/desk_text.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned state=91413;
static unsigned random_value(void) { state=state*1664525u+1013904223u;return state; }
static unsigned fold(unsigned char ch) { return ch>='A' && ch<='Z'?ch+32:ch; }
int main(void) {
    char data[1024],history[1024],scratch[1024],journal_data[4096];struct desk_text_buffer t;struct desk_text_history journal;
    assert(!desk_text_buffer_init(&t,data,history,sizeof(data)));
    for(int journal_mode=0;journal_mode<2;journal_mode++) {
        if(journal_mode) assert(!desk_text_buffer_history(&t,&journal,journal_data,sizeof(journal_data)));
        desk_text_buffer_set(&t,"one ONE café one",17);desk_text_buffer_select(&t,7,4);size_t count=99;
        assert(desk_text_replace(&t,"one",3,"1",1,0,0,scratch,sizeof(scratch),&count)==1 && count==1 && !strcmp(t.data,"one 1 café one"));
        assert(desk_text_buffer_undo(&t,0)==1 && t.anchor==7 && t.caret==4 && !strcmp(t.data,"one ONE café one"));
        assert(desk_text_buffer_undo(&t,1)==1 && !strcmp(t.data,"one 1 café one"));
        desk_text_buffer_select(&t,1,1);uint64_t rev=t.revision;
        assert(!desk_text_replace(&t,"one",3,"",0,0,0,scratch,sizeof(scratch),&count) && !count && t.revision==rev);
        assert(desk_text_replace(&t,"one",3,"one one",7,0,1,scratch,sizeof(scratch),&count)==1 && count==2 && !strcmp(t.data,"one one 1 café one one"));
        assert(desk_text_buffer_undo(&t,0)==1 && !strcmp(t.data,"one 1 café one") && t.caret==1 && t.anchor==1);
        assert(desk_text_buffer_undo(&t,1)==1 && !strcmp(t.data,"one one 1 café one one"));
        desk_text_buffer_set(&t,"aaaaa",5);
        assert(desk_text_replace(&t,"aa",2,"b",1,1,1,scratch,sizeof(scratch),&count)==1 && count==2 && !strcmp(t.data,"bba"));
        assert(desk_text_buffer_undo(&t,0)==1 && !strcmp(t.data,"aaaaa"));
        desk_text_buffer_set(&t,"éé É",7);
        assert(desk_text_replace(&t,"é",2,"",0,0,1,scratch,sizeof(scratch),&count)==1 && count==2 && !strcmp(t.data," É"));
        desk_text_buffer_select(&t,0,1);rev=t.revision;unsigned flags=t.history;
        assert(desk_text_replace(&t,"É",2,"É",2,1,1,scratch,sizeof(scratch),&count)==0 && count==1 && t.revision==rev && flags==t.history && t.anchor==0 && t.caret==1);
        assert(desk_text_replace(&t,"\xc0",1,"x",1,1,1,scratch,sizeof(scratch),&count)<0);
        assert(desk_text_replace(&t,"É",2,"\xff",1,1,1,scratch,sizeof(scratch),&count)<0);
        assert(desk_text_replace(&t,"É",2,"xxx",3,1,1,scratch,3,&count)<0 && !count && !strcmp(t.data," É") && t.revision==rev && t.history==flags);
        assert(desk_text_replace(&t,"É",2,"x",1,1,1,t.data,t.capacity,&count)<0 && !strcmp(t.data," É"));
        assert(desk_text_replace(&t,"É",2,"x",1,1,1,t.history_data,t.capacity,&count)<0);
        strcpy(scratch,"query");assert(desk_text_replace(&t,scratch,5,"",0,1,1,scratch,sizeof(scratch),&count)<0);
        assert(desk_text_buffer_replace_range(&t,2,3,"x",1)<0); /* continuation byte */
        desk_text_buffer_set(&t,"ABC",3);desk_text_buffer_select(&t,1,2);
        assert(desk_text_replace(&t,"abc",3,"x",1,1,1,scratch,sizeof(scratch),&count)==0 && !count && t.anchor==1 && t.caret==2);
        /* Independent sequential literal replacement; generated matches must not
         * be searched again. Check exactly one undo for the entire operation. */
        for(int trial=0;trial<5000;trial++) {
            char source[70],query[5],replacement[6],expected[1024];size_t n=random_value()%65,qn=random_value()%4+1,rn=random_value()%5;
            for(size_t i=0;i<n;i++) source[i]="aAbB"[random_value()%4];
            source[n]=0;
            for(size_t i=0;i<qn;i++) query[i]="aAbB"[random_value()%4];
            query[qn]=0;
            for(size_t i=0;i<rn;i++) replacement[i]="aAbBc"[random_value()%5];
            replacement[rn]=0;
            int sensitive=random_value()%2;size_t used=0,total=0;
            for(size_t at=0;at<n;) {
                int equal=at+qn<=n;
                for(size_t i=0;equal && i<qn;i++) equal=(sensitive?(unsigned)source[at+i]:fold(source[at+i]))==(sensitive?(unsigned)query[i]:fold(query[i]));
                if(equal) { memcpy(expected+used,replacement,rn);used+=rn;at+=qn;total++; }
                else expected[used++]=source[at++];
            }
            expected[used]=0;desk_text_buffer_set(&t,source,n);size_t anchor=random_value()%(n+1),caret=random_value()%(n+1);desk_text_buffer_select(&t,anchor,caret);
            int changed=strcmp(source,expected)!=0;
            assert(desk_text_replace(&t,query,qn,replacement,rn,sensitive,1,scratch,sizeof(scratch),&count)==changed && count==total && !strcmp(t.data,expected));
            if(changed) { assert(desk_text_buffer_undo(&t,0)==1 && !strcmp(t.data,source) && t.anchor==anchor && t.caret==caret);assert(desk_text_buffer_undo(&t,1)==1 && !strcmp(t.data,expected)); }
        }
    }
    char small[8],old[8];desk_text_buffer_init(&t,small,old,sizeof(small));desk_text_buffer_set(&t,"aa",2);desk_text_buffer_select(&t,1,2);size_t count;
    assert(desk_text_replace(&t,"a",1,"xxxx",4,1,1,scratch,sizeof(scratch),&count)<0 && !strcmp(t.data,"aa") && t.anchor==1 && t.caret==2 && !t.history);
    puts("text-replace: PASS literal selection/all, ASCII case, UTF-8, nonoverlap, empty replacement, atomic limits/alias rejection, no-op and 10,000 independent undo/redo comparisons");
}
