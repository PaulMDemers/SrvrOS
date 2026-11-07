#define main old_owner_fixture_main
#include "test_intel_hda_owner.c"
#undef main
int main(int argc,char **argv) {
 const char *mode=argc>1?argv[1]:"normal";struct model m={.mode="normal",.t=100,.command=6};
 struct intel_hda_owner_result r={0};struct intel_hda_owner_token t={0};struct intel_hda_owner_borrow b={0},b2={0};struct intel_hda_result mid={0};
 struct intel_hda_owner_facts f={true,true,true,true,true,true,true,true};struct intel_hda_owner_io io={&m,inspect,guard,endpoint,write16,now};
 assert(intel_hda_owner_open(&r,f,io,&t));assert(m.command==2 && m.stores==1 && r.phases==2 && !r.complete && !r.retained && intel_hda_owner_live(&r,&t));
 struct intel_hda_owner_token bad=t;bad.epoch++;
 struct intel_hda_owner_result copy=r;unsigned reads=m.inspects,guards=m.guards;
 assert(!intel_hda_owner_pin(&r,&bad,&b) && !intel_hda_owner_close(&r,&bad) && !intel_hda_owner_unpin(&r,&b));
 assert(!intel_hda_owner_pin(&copy,&t,&b) && !intel_hda_owner_close(&copy,&t) && !intel_hda_owner_check(&r,&b,&r.phase[0]));
 assert(!intel_hda_owner_check(&r,&b,&mid) && !intel_hda_owner_unpin(&r,&b));assert(m.inspects==reads && m.guards==guards && m.stores==1);
 if(!strcmp(mode,"pin-guard"))m.mode="guard";
 bool pinned=intel_hda_owner_pin(&r,&t,&b);
 if(!strcmp(mode,"pin-guard")){assert(!pinned);goto failed;}
 assert(pinned && r.pins==1);
 struct intel_hda_owner_borrow copied=b;unsigned old_pins=r.pins;
 reads=m.inspects;guards=m.guards;
 assert(!intel_hda_owner_unpin(&r,&copied) && !intel_hda_owner_check(&r,&copied,&mid));
 assert(r.pins==old_pins && m.inspects==reads && m.guards==guards);
 reads=m.inspects;guards=m.guards;assert(!intel_hda_owner_close(&r,&t));assert(m.stores==1 && m.command==2 && m.inspects==reads && m.guards==guards && r.pins==1 && intel_hda_owner_live(&r,&t));
 if(!strcmp(mode,"two-pins"))assert(intel_hda_owner_pin(&r,&t,&b2) && r.pins==2);
 if(!strcmp(mode,"check-guard"))m.mode="guard";
 if(!strcmp(mode,"check-power"))m.mode="power-change";
 if(!strcmp(mode,"check-active"))m.mode="late-active";
 if(!strcmp(mode,"check-retire"))m.mode="map-retained";
 if(!strcmp(mode,"check-timeout"))m.t+=400001;
 bool checked=intel_hda_owner_check(&r,&b,&mid);
 if(strncmp(mode,"check-",6)==0){assert(!checked);goto failed;}
 assert(checked && r.checks==1 && intel_hda_owner_live(&r,&t) && m.command==2 && m.stores==1);
 reads=m.inspects;guards=m.guards;assert(!intel_hda_owner_check(&r,&b,&mid));assert(m.inspects==reads && m.guards==guards);
 if(!strcmp(mode,"unpin-guard"))m.mode="guard";
 bool unpinned=intel_hda_owner_unpin(&r,&b);
 if(!strcmp(mode,"unpin-guard")){assert(!unpinned);goto failed;}
 assert(unpinned);
 if(!strcmp(mode,"two-pins")){assert(r.pins==1 && !intel_hda_owner_close(&r,&t) && m.stores==1);assert(!intel_hda_owner_unpin(&r,&b) && r.pins==1);assert(intel_hda_owner_unpin(&r,&b2));}
 assert(!r.pins && !intel_hda_owner_unpin(&r,&b));
 if(!strcmp(mode,"close-fail"))m.mode="restore-fail";
 /* Mutating the caller's I/O descriptor cannot replace the captured callbacks. */
 io.command16=NULL;
 bool closed=intel_hda_owner_close(&r,&t);
 if(!strcmp(mode,"close-fail")){assert(!closed && m.stores==2);goto failed;}
 assert(closed && m.stores==2 && m.command==6 && r.complete && r.restored && !r.retained && r.state==INTEL_HDA_OWNER_CLOSED);
 assert(!intel_hda_owner_live(&r,&t) && !intel_hda_owner_pin(&r,&t,&b) && !intel_hda_owner_close(&r,&t));
 puts("hda-lease: PASS pinned dependent check, blocked restore, sealed token/context and clean close");return 0;
failed:
 assert(r.retained && r.state==INTEL_HDA_OWNER_FAULT && !r.complete && m.command==2);
 reads=m.inspects;guards=m.guards;unsigned stores=m.stores;
 assert(!intel_hda_owner_live(&r,&t) && !intel_hda_owner_close(&r,&t) && !intel_hda_owner_unpin(&r,&b) && !intel_hda_owner_check(&r,&b,&mid));
 assert(m.inspects==reads && m.guards==guards && m.stores==stores);
 puts("hda-lease: PASS retained inhibition on dependent fault, no retries or restore");return 0;
}
