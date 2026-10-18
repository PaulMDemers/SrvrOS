#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/pty_model.h>
static struct pty_model p;
static uint8_t bytes[PTY_BYTES+1];
static int64_t write_text(bool master,const char *text) { return pty_model_write(&p,master,(const uint8_t *)text,strlen(text)); }
static int64_t read_bytes(bool master,unsigned size) { return pty_model_read(&p,master,bytes,size,true); }
static void drain(void) { while(read_bytes(true,PTY_BYTES)>0) {} }
static void check(void) {
    assert(p.input.size<=PTY_BYTES && p.output.size<=PTY_BYTES && p.line_size<PTY_LINE && p.record_count<=PTY_RECORDS);
    if(p.attributes.lflag&SRV_TTY_LFLAG_ICANON) {
        unsigned sum=0;for(unsigned i=0;i<p.record_count;i++) sum+=p.records[(p.record_head+i)%PTY_RECORDS];
        assert(sum==p.input.size);
    }
}
int main(void) {
    pty_model_init(&p);assert(p.size.row==24 && p.size.col==80);
    assert(write_text(true,"hello")==5 && read_bytes(false,20)==SRV_ERR_AGAIN);
    assert(read_bytes(true,20)==5 && !memcmp(bytes,"hello",5));
    assert(write_text(true,"\rsecond\n")==8);
    assert(read_bytes(false,3)==3 && !memcmp(bytes,"hel",3));
    assert(read_bytes(false,20)==3 && !memcmp(bytes,"lo\n",3));
    assert(read_bytes(false,20)==7 && !memcmp(bytes,"second\n",7));drain();
    assert(write_text(true,"abc\177\n")==5);assert(read_bytes(false,20)==3 && !memcmp(bytes,"ab\n",3));drain();
    assert(write_text(true,"A\xc3\xa9\177\n")==5);assert(read_bytes(false,20)==2 && !memcmp(bytes,"A\n",2));drain();
    assert(write_text(true,"discard\025kept\n")==13);assert(read_bytes(false,20)==5 && !memcmp(bytes,"kept\n",5));drain();
    assert(write_text(true,"partial\004")==8);assert(read_bytes(false,20)==7 && !memcmp(bytes,"partial",7));
    assert(write_text(true,"\004")==1 && read_bytes(false,20)==0 && read_bytes(false,20)==SRV_ERR_AGAIN);drain();
    assert(write_text(true,"drop\003")==5 && p.signals==(1ull<<SRV_SIGNAL_INT) && !p.line_size && !p.input.size);
    p.signals=0;drain();
    struct srv_termios raw=p.attributes;raw.lflag=0;raw.iflag=0;raw.oflag=0;
    assert(!pty_model_attributes(&p,&raw,SRV_TCSANOW));
    uint8_t binary[]={0,3,4,127,255};assert(pty_model_write(&p,true,binary,5)==5);
    assert(read_bytes(false,20)==5 && !memcmp(bytes,binary,5) && !p.signals && !p.output.size);
    raw.cc[SRV_TTY_VMIN]=4;assert(!pty_model_attributes(&p,&raw,0));write_text(true,"abc");
    assert(pty_model_read(&p,false,bytes,8,false)==SRV_ERR_AGAIN);
    assert(read_bytes(false,8)==3);raw.cc[SRV_TTY_VMIN]=0;assert(!pty_model_attributes(&p,&raw,0));
    assert(pty_model_read(&p,false,bytes,8,false)==0 && read_bytes(false,8)==SRV_ERR_AGAIN);
    memset(bytes,'x',PTY_BYTES);assert(pty_model_write(&p,false,bytes,PTY_BYTES)==PTY_BYTES);
    assert(write_text(false,"x")==SRV_ERR_AGAIN && !(pty_model_poll(&p,false,SRV_POLLOUT)&SRV_POLLOUT));
    assert(pty_model_attributes(&p,&raw,SRV_TCSADRAIN)==SRV_ERR_AGAIN);drain();
    assert(!pty_model_attributes(&p,&raw,SRV_TCSADRAIN));
    raw.cc[SRV_TTY_VTIME]=1;assert(pty_model_attributes(&p,&raw,0)<0);raw.cc[SRV_TTY_VTIME]=0;
    pty_model_init(&p);assert(write_text(false,"a\nb")==3);assert(read_bytes(true,20)==4 && !memcmp(bytes,"a\r\nb",4));
    assert(write_text(true,"pending")==7);drain();raw=p.attributes;raw.lflag=0;
    assert(!pty_model_attributes(&p,&raw,0));assert(read_bytes(false,20)==7 && !memcmp(bytes,"pending",7));
    pty_model_init(&p);p.attributes.lflag&=~SRV_TTY_LFLAG_ECHO;
    for(unsigned i=0;i<PTY_RECORDS;i++) assert(write_text(true,"\004")==1);
    assert(write_text(true,"\004")==SRV_ERR_AGAIN);
    for(unsigned i=0;i<PTY_RECORDS;i++) assert(read_bytes(false,20)==0);
    memset(bytes,'x',PTY_BYTES);assert(pty_model_write(&p,true,bytes,PTY_BYTES)==PTY_BYTES && p.line_size==PTY_LINE-1);
    assert(write_text(true,"\n")==1 && read_bytes(false,PTY_BYTES)==PTY_LINE);
    assert(write_text(true,"tail")==4);p.master_closed=true;
    assert(read_bytes(false,20)==4 && !memcmp(bytes,"tail",4));assert(read_bytes(false,20)==0 && write_text(false,"x")<0);
    assert(pty_model_poll(&p,false,SRV_POLLIN)&SRV_POLLHUP);
    pty_model_init(&p);p.slave_closed=true;assert(read_bytes(true,20)==0 && write_text(true,"x")<0);
    /* Deterministic adversarial streams keep all bounded rings consistent. */
    pty_model_init(&p);uint32_t random=0x12345678;
    for(unsigned i=0;i<100000;i++) {
        random=random*1664525+1013904223;uint8_t c=random>>24;
        if(random&1) {
            bool ready=pty_model_write_ready(&p,true,c);
            int64_t written=pty_model_write(&p,true,&c,1);
            assert(ready==(written==1));
        } else if(random&2) read_bytes(false,random%19+1);else read_bytes(true,random%31+1);
        p.signals=0;check();
    }
    puts("pty-model: PASS canonical records/EOF, UTF-8 erase, echo, raw bytes, signals, VMIN, backpressure, mode transitions, close and fuzz invariants");
}
