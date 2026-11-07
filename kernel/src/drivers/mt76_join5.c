#include <srvros/mt76_join5.h>
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool equal(const uint8_t *a,const uint8_t *b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
static bool send(struct mt76_join5 *s,const struct mt76_mgmt5_peer *p,const struct mt76_join5_io *io,enum mt76_mgmt5_kind kind){
 if(s->unsafe || !io->owned(io->context,kind==MT76_MGMT5_LEAVE))return false;
 size_t n=mt76_mgmt5_usb_request(p,kind,(unsigned)kind+1,false,s->packet,sizeof s->packet);
 if(!n)return false;
 s->sends++;enum mt76_join5_send result=io->send(io->context,kind,s->packet,n);
 scrub(s->packet,sizeof s->packet);
 if(result==MT76_JOIN5_UNCERTAIN || (result!=MT76_JOIN5_SENT && result!=MT76_JOIN5_REFUSED)){s->unsafe=true;return false;}
 if(!io->owned(io->context,kind==MT76_MGMT5_LEAVE)){s->unsafe=true;return false;}
 return result==MT76_JOIN5_SENT;
}
static bool response(struct mt76_join5 *s,const struct mt76_mgmt5_peer *p,const struct mt76_join5_io *io,enum mt76_mgmt5_kind kind){
 uint64_t start=io->now(io->context),previous=start;
 for(unsigned attempt=0;attempt<8192;attempt++){
  if(!io->owned(io->context,false)){s->unsafe=true;return false;}
  size_t n=0;bool present=false;s->polls++;
  if(!io->poll(io->context,s->reply,sizeof s->reply,&n,&present) || !io->owned(io->context,false)){s->unsafe=true;return false;}
  if(n>sizeof s->reply || (!present && n)){s->unsafe=true;return false;}
  if(present){
   if(n<24){s->ignored++;}
   else if((s->reply[0]&0xfc)!=(kind==MT76_MGMT5_AUTH?0xb0:0x10) || !equal(s->reply+4,p->station,6) || !equal(s->reply+10,p->bssid,6) || !equal(s->reply+16,p->bssid,6)){s->ignored++;}
   else {
    struct mt76_mgmt5_response r={0};enum mt76_mgmt5_status status=mt76_mgmt5_response(p,kind,s->reply,n,&r);s->status=r.status;
    scrub(s->reply,sizeof s->reply);
    if(status!=MT76_MGMT5_OK)return false;
    if(kind==MT76_MGMT5_ASSOC)s->aid=r.aid;
    return true;
   }
  }
  scrub(s->reply,sizeof s->reply);
  uint64_t now=io->now(io->context);
  if(now<previous || now-start>=MT76_JOIN5_REPLY_US)return false;
  previous=now;
  if(!io->delay(io->context,100)){s->unsafe=true;return false;}
 }
 return false;
}
bool mt76_join5_run_after(struct mt76_join5 *s,const struct mt76_peer5 *peer,const struct mt76_join5_io *io,bool (*after_assoc)(void *,bool *)){
 if(!s || !peer || !io || !io->owned || !io->send || !io->poll || !io->delay || !io->now ||
 overlap(s,sizeof *s,peer,sizeof *peer) || overlap(s,sizeof *s,io,sizeof *io) || (io->context && overlap(s,sizeof *s,io->context,1)) ||
 s->attempted || !peer->attempted || !peer->complete || !peer->generation || !peer->observations || peer->observations>8 ||
 !(peer->peer.basic_rates&1) || (peer->peer.basic_rates&~peer->rates))return false;
 const struct mt76_mgmt5_peer p=peer->peer;
 *s=(struct mt76_join5){.attempted=true,.generation=peer->generation};
 bool ok=io->owned(io->context,false);
 if(ok){s->auth_sent=send(s,&p,io,MT76_MGMT5_AUTH);ok=s->auth_sent;}
 if(ok){s->auth_matched=response(s,&p,io,MT76_MGMT5_AUTH);ok=s->auth_matched;}
 if(ok){s->assoc_sent=send(s,&p,io,MT76_MGMT5_ASSOC);ok=s->assoc_sent;}
 if(ok){s->assoc_matched=response(s,&p,io,MT76_MGMT5_ASSOC);ok=s->assoc_matched;}
 if(ok && after_assoc){ok=after_assoc(io->context,&s->unsafe);if(!io->owned(io->context,false)){s->unsafe=true;ok=false;}}
 if(s->auth_sent && !s->unsafe){s->leave_sent=send(s,&p,io,MT76_MGMT5_LEAVE);ok=s->leave_sent && ok;}
 else ok=false;
 scrub(s->packet,sizeof s->packet);scrub(s->reply,sizeof s->reply);
 s->complete=ok && s->auth_matched && s->assoc_matched && s->leave_sent && io->owned(io->context,true);
 s->failed=!s->complete;return s->complete;
}

bool mt76_join5_run(struct mt76_join5 *s,const struct mt76_peer5 *p,const struct mt76_join5_io *io){return mt76_join5_run_after(s,p,io,0);}
