#include <srvros/mt76_wpa5.h>
#include <srvros/mt76_eapol5.h>
#include <srvros/sha256.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool zero(const void *p,size_t n){const uint8_t *v=p;unsigned any=0;while(n--)any|=*v++;return !any;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){unsigned d=0;while(n--)d|=*a++^*b++;return !d;}
static bool owned(struct mt76_wpa5 *s,const struct mt76_wpa5_io *io,bool cleanup){
 if(s->unsafe)return false;
 if(io->owned(io->context,cleanup))return true;
 s->unsafe=true;return false;
}
static bool transmit(struct mt76_wpa5 *s,const struct mt76_peer5 *p,const struct mt76_wpa5_io *io,enum mt76_wpa5_tx kind){
 if(!owned(s,io,kind==MT76_WPA5_LEAVE))return false;
 bool key=kind==MT76_WPA5_M2 || kind==MT76_WPA5_M4;size_t n=0;
 if(key){
  size_t e=wpa2_hs_emit(&s->handshake,s->generation,s->eapol,sizeof s->eapol);
  struct mt76_eapol5_peer ep={.channel=161,.bandwidth=20};
  for(unsigned i=0;i<6;i++){ep.station[i]=p->peer.station[i];ep.bssid[i]=p->peer.bssid[i];}
  if(e)n=mt76_eapol5_usb_tx(&ep,s->eapol,e,(unsigned)kind+4,s->packet,sizeof s->packet);
  scrub(&ep,sizeof ep);scrub(s->eapol,sizeof s->eapol);
 }else{
  enum mt76_mgmt5_kind k=kind==MT76_WPA5_AUTH?MT76_MGMT5_AUTH:kind==MT76_WPA5_ASSOC?MT76_MGMT5_ASSOC:MT76_MGMT5_LEAVE;
  n=mt76_mgmt5_usb_request(&p->peer,k,(unsigned)kind+4,false,s->packet,sizeof s->packet);
 }
 if(!n)return false;
 s->sends++;enum mt76_join5_send result=io->send(io->context,kind,s->packet,n);
 scrub(s->packet,sizeof s->packet);
 if(result!=MT76_JOIN5_SENT && result!=MT76_JOIN5_REFUSED)s->unsafe=true;
 if(!owned(s,io,kind==MT76_WPA5_LEAVE))result=MT76_JOIN5_UNCERTAIN;
 if(key){enum wpa2_hs_result r=result==MT76_JOIN5_SENT?WPA2_HS_CONFIRMED:result==MT76_JOIN5_REFUSED?WPA2_HS_REFUSED:WPA2_HS_UNCERTAIN;
  return wpa2_hs_tx_result(&s->handshake,s->generation,r) && !s->unsafe;
 }
 return result==MT76_JOIN5_SENT && !s->unsafe;
}
static bool matches_m1(const uint8_t *reply,size_t n,const uint8_t digest[32]){
 if(n!=99 && n!=121)return false;
 uint8_t received[32];sha256_digest(reply,n,received);
 bool matched=equal(received,digest,sizeof received);
 scrub(received,sizeof received);return matched;
}
static bool response(struct mt76_wpa5 *s,const struct mt76_peer5 *p,const struct mt76_wpa5_io *io,enum mt76_wpa5_rx type,bool auth,uint8_t *m1_digest){
 uint64_t start=io->now(io->context),previous=start;
 unsigned stage=type==MT76_WPA5_MANAGEMENT?(auth?0:1):(s->m1_matched?3:2);
 struct mt76_wpa5_response *r=&s->responses[stage];r->start=r->last=start;
 unsigned limit=type==MT76_WPA5_EAPOL?1500000:MT76_JOIN5_REPLY_US;
 for(unsigned attempt=0;attempt<20000;attempt++){
  if(!owned(s,io,false)){r->result=MT76_WPA5_RESPONSE_OWNER;return false;}
  size_t n=0;bool present=false;s->polls++;r->polls++;
  if(!io->poll(io->context,type,s->reply,sizeof s->reply,&n,&present)){r->result=MT76_WPA5_RESPONSE_POLL;s->unsafe=true;return false;}
  if(!owned(s,io,false)){r->result=MT76_WPA5_RESPONSE_OWNER;s->unsafe=true;return false;}
  if(n>sizeof s->reply || (!present && n)){r->result=MT76_WPA5_RESPONSE_SPAN;s->unsafe=true;return false;}
  uint64_t now=io->now(io->context);
  r->last=now;
  if(now<previous){r->result=MT76_WPA5_RESPONSE_CLOCK;return false;}
  if(now-start>=limit){r->result=MT76_WPA5_RESPONSE_TIMEOUT;return false;}
  previous=now;
  if(present){
   r->present++;
   if(type==MT76_WPA5_EAPOL){
    unsigned info=n>=7?(unsigned)s->reply[5]*256+s->reply[6]:0;
    unsigned kind=info==0x008a?1:info==0x13ca?2:3;
    if(stage==3 && m1_digest && !r->ignored && kind==1 && matches_m1(s->reply,n,m1_digest)){
     s->ignored++;r->ignored++;
    }else{
     bool ok=wpa2_hs_receive(&s->handshake,s->generation,p->peer.bssid,p->peer.station,s->reply,n);
     if(ok && stage==2 && m1_digest)sha256_digest(s->reply,n,m1_digest);
     r->kind=kind;r->reject=(unsigned)s->handshake.last_rx;
     r->result=ok?MT76_WPA5_RESPONSE_OK:MT76_WPA5_RESPONSE_REJECTED;
     scrub(s->reply,sizeof s->reply);return ok;
    }
   }else if(n<24 || (s->reply[0]&0xfc)!=(auth?0xb0:0x10) || !equal(s->reply+4,p->peer.station,6) || !equal(s->reply+10,p->peer.bssid,6) || !equal(s->reply+16,p->peer.bssid,6)){s->ignored++;r->ignored++;}
   else{
    struct mt76_mgmt5_response reply={0};enum mt76_mgmt5_status status=mt76_mgmt5_response(&p->peer,auth?MT76_MGMT5_AUTH:MT76_MGMT5_ASSOC,s->reply,n,&reply);
    s->status=reply.status;if(!auth)s->aid=reply.aid;scrub(s->reply,sizeof s->reply);
    r->result=status==MT76_MGMT5_OK?MT76_WPA5_RESPONSE_OK:MT76_WPA5_RESPONSE_REJECTED;return status==MT76_MGMT5_OK;
   }
  }
  scrub(s->reply,sizeof s->reply);
  if(!io->delay(io->context,100)){r->result=MT76_WPA5_RESPONSE_DELAY;s->unsafe=true;return false;}
 }
 r->result=MT76_WPA5_RESPONSE_LIMIT;return false;
}
static bool handshake(struct mt76_wpa5 *s,const struct mt76_peer5 *p,const uint8_t pmk[32],
                      const struct mt76_wpa5_io *io,bool defer_m1_duplicate){
 s->attempted=true;s->generation=p->generation;
 bool ok=owned(s,io,false);
 if(ok)ok=io->nonce(io->context,s->snonce) && owned(s,io,false);
 if(ok){
  struct wpa2_hs_peer hp={.rsne_size=p->peer.rsn_size};
  for(unsigned i=0;i<6;i++){hp.aa[i]=p->peer.bssid[i];hp.spa[i]=p->peer.station[i];}
  for(unsigned i=0;i<hp.rsne_size;i++)hp.rsne[i]=p->peer.rsn[i];
  ok=wpa2_hs_start(&s->handshake,&hp,pmk,s->snonce,s->generation);scrub(&hp,sizeof hp);
 }
 scrub(s->snonce,sizeof s->snonce);
 uint8_t m1_digest[32]={0};
 uint8_t *duplicate_check=defer_m1_duplicate?m1_digest:NULL;
 if(ok){s->auth_sent=transmit(s,p,io,MT76_WPA5_AUTH);ok=s->auth_sent;}
 if(ok){s->auth_matched=response(s,p,io,MT76_WPA5_MANAGEMENT,true,NULL);ok=s->auth_matched;}
 if(ok){s->assoc_sent=transmit(s,p,io,MT76_WPA5_ASSOC);ok=s->assoc_sent;}
 if(ok){s->assoc_matched=response(s,p,io,MT76_WPA5_MANAGEMENT,false,NULL);ok=s->assoc_matched;}
 if(ok){s->m1_matched=response(s,p,io,MT76_WPA5_EAPOL,false,duplicate_check);ok=s->m1_matched;}
 if(ok){s->m2_sent=transmit(s,p,io,MT76_WPA5_M2);ok=s->m2_sent;}
 if(ok){s->m3_mic=response(s,p,io,MT76_WPA5_EAPOL,false,duplicate_check);ok=s->m3_mic;}
 scrub(m1_digest,sizeof m1_digest);
 if(ok){
  struct wpa2_hs_keys export={0};ok=owned(s,io,false) && wpa2_hs_take_keys(&s->handshake,s->generation,&export);
  if(ok){s->keys_installed=wpa2_ccmp_install(&s->keys,&s->handshake,&export);ok=s->keys_installed;}
  scrub(&export,sizeof export);
  if(ok)ok=wpa2_hs_install_result(&s->handshake,s->generation,WPA2_HS_CONFIRMED);
 }
 if(ok){s->m4_sent=transmit(s,p,io,MT76_WPA5_M4);ok=s->m4_sent;}
 if(ok){s->keys_activated=owned(s,io,false) && wpa2_ccmp_activate(&s->keys,&s->handshake);ok=s->keys_activated;}
 return ok;
}
static bool finish(struct mt76_wpa5 *s,const struct mt76_peer5 *p,const struct mt76_wpa5_io *io,
                   bool data,bool ok){
 /* Software erasure is independent of uncertain DMA. DMA must be retained
  * externally;none of these keys were published in a hardware key table. */
 if(s->keys.phase!=WPA2_CCMP_EMPTY)s->keys_erased=wpa2_ccmp_retire(&s->keys,s->generation) && wpa2_ccmp_erased(&s->keys,s->generation);
 else s->keys_erased=zero(&s->keys,sizeof s->keys);
 if(data && s->keys_activated && !s->keys_erased){
  /* Defensive local forget on a callback lifetime violation. This owner has
   * no hardware key table;it cannot authorize transport cleanup afterward. */
  scrub(&s->keys,sizeof s->keys);s->keys.generation=s->generation;s->keys.phase=WPA2_CCMP_CLOSED;
  s->keys_erased=true;s->unsafe=true;ok=false;
 }
 if(s->handshake.phase!=WPA2_HS_EMPTY || (data && s->keys_activated)){
  bool retired=wpa2_hs_retire(&s->handshake,s->generation,s->keys_erased);
  if(!retired && data){
   /* An extension contract violation cannot retain local secrets or become
    * a successful handshake. No hardware keys exist in this owner. */
   scrub(&s->handshake,sizeof s->handshake);s->handshake.generation=s->generation;s->handshake.phase=WPA2_HS_FAILED;
   s->handshake.keys_may_live=!s->keys_erased;s->unsafe=true;
  }
  ok=retired && ok;
 }
 ok=s->keys_erased && ok;
 if(s->auth_sent && !s->unsafe){s->leave_sent=transmit(s,p,io,MT76_WPA5_LEAVE);ok=s->leave_sent && ok;}else ok=false;
 scrub(s->reply,sizeof s->reply);scrub(s->packet,sizeof s->packet);scrub(s->eapol,sizeof s->eapol);scrub(s->snonce,sizeof s->snonce);
 s->complete=ok && !s->unsafe && s->keys_activated && s->leave_sent && owned(s,io,true);return s->complete;
}
static bool admissible(struct mt76_wpa5 *s,const struct mt76_peer5 *p,const uint8_t pmk[32],
                       const struct mt76_wpa5_io *io){
 if(!s || !p || !pmk || !io || !io->owned || !io->nonce || !io->send || !io->poll || !io->delay || !io->now ||
    overlap(s,sizeof *s,p,sizeof *p) || overlap(s,sizeof *s,pmk,32) || overlap(s,sizeof *s,io,sizeof *io) ||
    (io->context && overlap(s,sizeof *s,io->context,1)) || !zero(s,sizeof *s) ||
    !p->attempted || !p->complete || !p->generation || !p->observations || p->observations>8 ||
    !(p->peer.basic_rates&1) || (p->peer.basic_rates&~p->rates) || p->peer.rsn_size>64)return false;
 uint8_t check[124]={0};
 size_t valid=mt76_mgmt5_usb_request(&p->peer,MT76_MGMT5_AUTH,4,false,check,sizeof check);
 scrub(check,sizeof check);
 return valid!=0;
}
static void clear_session(struct mt76_wpa5 *s){
 scrub(&s->session_io,sizeof s->session_io);
 scrub(&s->session_peer,sizeof s->session_peer);
 s->session_identity=0;
 s->session_busy=false;
}
bool mt76_wpa5_connect(struct mt76_wpa5 *s,const struct mt76_peer5 *p,const uint8_t pmk[32],
                       const struct mt76_wpa5_io *io,bool defer_m1_duplicate){
 if(!admissible(s,p,pmk,io))return false;
 s->session_identity=(uintptr_t)s;
 s->session_io=*io;
 s->session_peer=*p;
 if(handshake(s,&s->session_peer,pmk,&s->session_io,defer_m1_duplicate)){
  s->session_active=true;
  return true;
 }
 (void)finish(s,&s->session_peer,&s->session_io,true,false);
 clear_session(s);
 return false;
}
static bool finish_session(struct mt76_wpa5 *s,uint64_t generation,enum mt76_join5_send result){
 s->session_active=false;
 if((result!=MT76_JOIN5_SENT && result!=MT76_JOIN5_REFUSED) ||
    s->generation!=generation || s->keys.phase!=WPA2_CCMP_ACTIVE || s->keys.generation!=generation ||
    s->handshake.phase!=WPA2_HS_COMPLETE || s->handshake.generation!=generation ||
    s->handshake.key_owner!=(uintptr_t)&s->keys)s->unsafe=true;
 s->generation=generation;
 bool ok=result==MT76_JOIN5_SENT && owned(s,&s->session_io,false);
 ok=finish(s,&s->session_peer,&s->session_io,true,ok);
 clear_session(s);
 return ok;
}
bool mt76_wpa5_disconnect(struct mt76_wpa5 *s,uint64_t generation,enum mt76_join5_send result){
 if(!s || s->session_identity!=(uintptr_t)s || !s->session_active || s->session_busy ||
    !generation || generation!=s->session_peer.generation)return false;
 return finish_session(s,generation,result);
}
bool mt76_wpa5_run(struct mt76_wpa5 *s,const struct mt76_peer5 *p,const uint8_t pmk[32],
                   const struct mt76_wpa5_io *io){
 if(!admissible(s,p,pmk,io))return false;
 return finish(s,p,io,false,handshake(s,p,pmk,io,false));
}
bool mt76_wpa5_run_data(struct mt76_wpa5 *s,const struct mt76_peer5 *p,const uint8_t pmk[32],
                        const struct mt76_wpa5_io *io,const struct mt76_wpa5_data *data){
 if(!s || !data || !data->run || overlap(s,sizeof *s,data,sizeof *data) ||
    (data->context && overlap(s,sizeof *s,data->context,1)) ||
    !mt76_wpa5_connect(s,p,pmk,io,data->defer_m1_duplicate))return false;
 enum mt76_join5_send result=MT76_JOIN5_UNCERTAIN;
 uint64_t generation=s->generation;
 if(owned(s,&s->session_io,false)){
  s->session_busy=true;
  result=data->run(data->context,&s->keys,generation,p);
  s->session_busy=false;
 }
 return finish_session(s,generation,result);
}
