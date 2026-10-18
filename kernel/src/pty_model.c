#include <srvros/pty_model.h>

static uint32_t space(const struct pty_ring *r) { return PTY_BYTES-r->size; }
static void put(struct pty_ring *r,uint8_t byte) { r->data[(r->head+r->size)%PTY_BYTES]=byte;r->size++; }
static uint8_t get(struct pty_ring *r) { uint8_t c=r->data[r->head];r->head=(r->head+1)%PTY_BYTES;r->size--;return c; }
static void clear_input(struct pty_model *p) {
    p->input.head=p->input.size=p->line_size=p->record_head=p->record_count=0;
}
void pty_model_init(struct pty_model *p) {
    *p=(struct pty_model){0};
    p->attributes.iflag=SRV_TTY_IFLAG_ICRNL;
    p->attributes.oflag=SRV_TTY_OFLAG_OPOST|SRV_TTY_OFLAG_ONLCR;
    p->attributes.lflag=SRV_TTY_LFLAG_ECHO|SRV_TTY_LFLAG_ICANON|SRV_TTY_LFLAG_ISIG;
    p->attributes.cc[SRV_TTY_VEOF]=4;p->attributes.cc[SRV_TTY_VEOL]='\n';
    p->attributes.cc[SRV_TTY_VERASE]=127;p->attributes.cc[SRV_TTY_VINTR]=3;
    p->attributes.cc[SRV_TTY_VKILL]=21;p->attributes.cc[SRV_TTY_VMIN]=1;
    p->size=(struct srv_winsize){.row=24,.col=80};
}
static unsigned output_width(const struct pty_model *p,uint8_t c) {
    return c=='\n' && (p->attributes.oflag&(SRV_TTY_OFLAG_OPOST|SRV_TTY_OFLAG_ONLCR))==
        (SRV_TTY_OFLAG_OPOST|SRV_TTY_OFLAG_ONLCR)?2:1;
}
static void output(struct pty_model *p,uint8_t c) {
    if(output_width(p,c)==2) put(&p->output,'\r');
    put(&p->output,c);
}
static bool publish(struct pty_model *p,bool newline,uint8_t c) {
    unsigned length=p->line_size+(newline?1:0);
    if(p->record_count==PTY_RECORDS || space(&p->input)<length) return false;
    for(unsigned i=0;i<p->line_size;i++) put(&p->input,p->line[i]);
    if(newline) put(&p->input,c);
    p->records[(p->record_head+p->record_count)%PTY_RECORDS]=length;p->record_count++;p->line_size=0;
    return true;
}
static unsigned erased_bytes(const struct pty_model *p) {
    if(!p->line_size) return 0;
    unsigned start=p->line_size-1;
    while(start && (p->line[start]&0xc0)==0x80) start--;
    return p->line_size-start;
}
bool pty_model_write_ready(const struct pty_model *p,bool master,uint8_t c) {
    if(master?p->slave_closed:p->master_closed) return true;
    if(!master) return space(&p->output)>=output_width(p,c);
    bool echo=(p->attributes.lflag&SRV_TTY_LFLAG_ECHO)!=0;
    if(c=='\r' && (p->attributes.iflag&SRV_TTY_IFLAG_ICRNL)) c='\n';
    if((p->attributes.lflag&SRV_TTY_LFLAG_ISIG) && p->attributes.cc[SRV_TTY_VINTR] && c==p->attributes.cc[SRV_TTY_VINTR])
        return !echo || space(&p->output)>=4;
    if(!(p->attributes.lflag&SRV_TTY_LFLAG_ICANON))
        return space(&p->input) && (!echo || space(&p->output)>=output_width(p,c));
    if(p->attributes.cc[SRV_TTY_VERASE] && c==p->attributes.cc[SRV_TTY_VERASE])
        return !p->line_size || !echo || space(&p->output)>=3;
    if(p->attributes.cc[SRV_TTY_VKILL] && c==p->attributes.cc[SRV_TTY_VKILL])
        return !echo || space(&p->output)>=p->line_size*3;
    if(p->attributes.cc[SRV_TTY_VEOF] && c==p->attributes.cc[SRV_TTY_VEOF])
        return p->record_count<PTY_RECORDS && space(&p->input)>=p->line_size;
    if(c=='\n' || (p->attributes.cc[SRV_TTY_VEOL] && c==p->attributes.cc[SRV_TTY_VEOL]))
        return p->record_count<PTY_RECORDS && space(&p->input)>=p->line_size+1 &&
            (!echo || space(&p->output)>=output_width(p,c));
    return p->line_size==PTY_LINE-1 || !echo || space(&p->output)>=output_width(p,c);
}
static bool input(struct pty_model *p,uint8_t c) {
    bool echo=(p->attributes.lflag&SRV_TTY_LFLAG_ECHO)!=0;
    bool canonical=(p->attributes.lflag&SRV_TTY_LFLAG_ICANON)!=0;
    if(c=='\r' && (p->attributes.iflag&SRV_TTY_IFLAG_ICRNL)) c='\n';
    if((p->attributes.lflag&SRV_TTY_LFLAG_ISIG) && p->attributes.cc[SRV_TTY_VINTR] && c==p->attributes.cc[SRV_TTY_VINTR]) {
        if(echo && space(&p->output)<4) return false;
        clear_input(p);p->signals|=1ull<<SRV_SIGNAL_INT;
        if(echo) { put(&p->output,'^');put(&p->output,'C');put(&p->output,'\r');put(&p->output,'\n'); }
        return true;
    }
    if(!canonical) {
        if(!space(&p->input) || (echo && space(&p->output)<output_width(p,c))) return false;
        put(&p->input,c);if(echo) output(p,c);
        return true;
    }
    if(p->attributes.cc[SRV_TTY_VERASE] && c==p->attributes.cc[SRV_TTY_VERASE]) {
        unsigned count=erased_bytes(p);if(!count) return true;
        if(echo && space(&p->output)<3) return false;
        p->line_size-=count;
        if(echo) { put(&p->output,'\b');put(&p->output,' ');put(&p->output,'\b'); }
        return true;
    }
    if(p->attributes.cc[SRV_TTY_VKILL] && c==p->attributes.cc[SRV_TTY_VKILL]) {
        if(echo && space(&p->output)<p->line_size*3) return false;
        while(p->line_size) {
            p->line_size-=erased_bytes(p);
            if(echo) { put(&p->output,'\b');put(&p->output,' ');put(&p->output,'\b'); }
        }
        return true;
    }
    if(p->attributes.cc[SRV_TTY_VEOF] && c==p->attributes.cc[SRV_TTY_VEOF]) return publish(p,false,0);
    if(c=='\n' || (p->attributes.cc[SRV_TTY_VEOL] && c==p->attributes.cc[SRV_TTY_VEOL])) {
        if(echo && space(&p->output)<output_width(p,c)) return false;
        if(!publish(p,true,c)) return false;
        if(echo) output(p,c);
        return true;
    }
    /* Reserve room for a terminator. Ignore further ordinary bytes at the
     * canonical limit; erase/newline/EOF remain usable instead of deadlocking. */
    if(p->line_size==PTY_LINE-1) return true;
    if(echo && space(&p->output)<output_width(p,c)) return false;
    p->line[p->line_size++]=c;if(echo) output(p,c);
        return true;
}
int64_t pty_model_write(struct pty_model *p,bool master,const uint8_t *data,uint64_t length) {
    if(!length) return 0;
    if(!data || (master?p->slave_closed:p->master_closed)) return -1;
    uint64_t done=0;
    while(done<length) {
        if(master) {
            if(!input(p,data[done])) break;
            done++;if(p->signals) break;
        } else {
            if(space(&p->output)<output_width(p,data[done])) break;
            output(p,data[done++]);
        }
    }
    return done?(int64_t)done:SRV_ERR_AGAIN;
}
int64_t pty_model_read(struct pty_model *p,bool master,uint8_t *data,uint64_t length,bool nonblocking) {
    if(!length) return 0;
    if(!data) return -1;
    struct pty_ring *r=master?&p->output:&p->input;
    if(!master && (p->attributes.lflag&SRV_TTY_LFLAG_ICANON)) {
        if(!p->record_count) {
            if(!p->master_closed) return SRV_ERR_AGAIN;
            /* The master can disappear with a partial canonical line. */
            uint64_t count=length<p->line_size?length:p->line_size;
            for(uint64_t i=0;i<count;i++) data[i]=p->line[i];
            for(uint64_t i=count;i<p->line_size;i++) p->line[i-count]=p->line[i];
            p->line_size-=count;return count;
        }
        uint16_t *record=&p->records[p->record_head];
        uint64_t count=length<*record?length:*record;
        for(uint64_t i=0;i<count;i++) data[i]=get(r);
        *record-=count;
        if(!*record) { p->record_head=(p->record_head+1)%PTY_RECORDS;p->record_count--; }
        return count;
    }
    unsigned minimum=master || nonblocking?1:p->attributes.cc[SRV_TTY_VMIN];
    if(minimum>length) minimum=length;
    bool closed=master?p->slave_closed:p->master_closed;
    if(r->size<minimum && !closed) return SRV_ERR_AGAIN;
    uint64_t count=length<r->size?length:r->size;
    for(uint64_t i=0;i<count;i++) data[i]=get(r);
    return count;
}
uint16_t pty_model_poll(const struct pty_model *p,bool master,uint16_t events) {
    uint16_t result=0;bool closed=master?p->slave_closed:p->master_closed;
    bool readable=master?p->output.size!=0:(p->attributes.lflag&SRV_TTY_LFLAG_ICANON)?p->record_count!=0:p->input.size!=0;
    bool writable=master?space(&p->input)>0 && space(&p->output)>=4:space(&p->output)>=2;
    if((events&SRV_POLLIN) && (readable || closed)) result|=SRV_POLLIN;
    if((events&SRV_POLLOUT) && writable && !closed) result|=SRV_POLLOUT;
    if(closed) result|=SRV_POLLHUP;
    return result;
}
int pty_model_attributes(struct pty_model *p,const struct srv_termios *a,unsigned action) {
    if(!a || action>SRV_TCSAFLUSH || a->cc[SRV_TTY_VTIME] ||
       (a->iflag&~SRV_TTY_IFLAG_ICRNL) || (a->oflag&~(SRV_TTY_OFLAG_OPOST|SRV_TTY_OFLAG_ONLCR)) ||
       (a->lflag&~(SRV_TTY_LFLAG_ECHO|SRV_TTY_LFLAG_ICANON|SRV_TTY_LFLAG_ISIG))) return -1;
    if(action!=SRV_TCSANOW && p->output.size) return SRV_ERR_AGAIN;
    bool was_canonical=(p->attributes.lflag&SRV_TTY_LFLAG_ICANON)!=0,canonical=(a->lflag&SRV_TTY_LFLAG_ICANON)!=0;
    if(action==SRV_TCSAFLUSH) clear_input(p);
    else if(was_canonical && !canonical) {
        if(space(&p->input)<p->line_size) return SRV_ERR_AGAIN;
        for(unsigned i=0;i<p->line_size;i++) put(&p->input,p->line[i]);
        p->line_size=p->record_head=p->record_count=0;
    } else if(!was_canonical && canonical && p->input.size) {
        p->records[0]=p->input.size;p->record_head=0;p->record_count=1;
    }
    p->attributes=*a;return 0;
}
