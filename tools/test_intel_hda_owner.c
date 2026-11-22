#include <srvros/intel_hda_owner.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct model {const char *mode;uint64_t t;unsigned command,stores,inspects,guards;};
static bool is(struct model *m,const char *s){return !strcmp(m->mode,s);}
static uint64_t now(void *p){struct model *m=p;if(is(m,"zero"))return 0;if(is(m,"frozen"))return 100;if(is(m,"backward"))return 10000-m->t++;return ++m->t;}
static bool endpoint(void *p,struct intel_hda_endpoint *e){struct model *m=p;*e=(struct intel_hda_endpoint){0x160c8086,0x04030009,0x011b106b,m->command,1,0,1,0xc1610004,0,true};if(is(m,"power-change") && m->stores)e->pm=3;if(is(m,"resource-change") && m->inspects)e->bar+=4096;return true;}
static bool guard(void *p){struct model *m=p;m->guards++;return !is(m,"guard") && !(is(m,"late-guard") && m->stores);}
static bool inspect(void *p,struct intel_hda_result *s){struct model *m=p;m->inspects++;*s=(struct intel_hda_result){.object=s,.samples=2,.reads=58,.maps=1,.retired=1,.stable=true,.complete=true,.begin_us=++m->t};endpoint(p,&s->endpoint);
 for(unsigned n=0;n<2;n++){s->sample[n].streams=3;s->sample[n].global[0]=0x3001;s->sample[n].global[1]=1;for(unsigned i=0;i<3;i++)s->sample[n].stream[i][0]=0x40000;}
 if(is(m,"active") || (is(m,"late-active") && m->stores))s->sample[1].stream[2][0]|=2;
 if(is(m,"buffer"))s->sample[0].stream[1][3]=4096;
 if(is(m,"corb"))s->sample[0].global[3]=4096;
 if(is(m,"intctl"))s->sample[1].global[2]=1;
 if(is(m,"reset"))s->sample[0].global[1]=0;
 if(is(m,"topology"))s->sample[1].streams=4;
 if(is(m,"changed"))s->stable=false;
 if(is(m,"self-copy"))s->object=(void *)1;
 if(is(m,"map-retained")){s->retained=true;s->retired=0;}
 s->end_us=++m->t;if(is(m,"inspect-slow"))m->t+=400001;return !is(m,"inspect-fail");}
static bool write16(void *p,uint16_t v){struct model *m=p;assert((m->command==6 && !m->stores && v==2) || (m->command==2 && m->stores==1 && v==6));m->stores++;
 if(is(m,"unissued") || (is(m,"restore-fail") && m->stores==2))return false;
 if(!is(m,"readback"))m->command=v;
 if(is(m,"uncertain"))return false;
 if(is(m,"write-slow"))m->t+=400001;
 return true;}
int main(int argc,char **argv){struct model m={.mode=argc>1?argv[1]:"normal",.t=100,.command=6};struct intel_hda_owner_result r={0};struct intel_hda_owner_facts f={true,true,true,true,true,true,true,true};struct intel_hda_owner_io io={&m,inspect,guard,endpoint,write16,now};
 bool *facts[]={&f.compiled,&f.recipe,&f.efi64,&f.loader,&f.profile,&f.serialized,&f.excluded,&f.prerequisites};
 if(is(&m,"facts")){for(unsigned i=0;i<8;i++){*facts[i]=false;struct intel_hda_owner_result x={0};assert(!intel_hda_owner_trial(&x,f,io) && !m.stores && !m.inspects);*facts[i]=true;}return 0;}
 bool ok=intel_hda_owner_trial(&r,f,io);assert(ok==is(&m,"normal"));
 if(ok)assert(r.phases==3 && r.attempts==2 && r.writes==2 && r.inhibited && r.restored && !r.retained && m.command==6);
 else {assert(r.reason);if(m.stores)assert(r.retained && !r.restored);else assert(r.retained==is(&m,"map-retained"));}
 if(is(&m,"unissued") || is(&m,"uncertain") || is(&m,"late-active") || is(&m,"power-change") || is(&m,"late-guard") || is(&m,"write-slow") || is(&m,"readback"))assert(m.stores==1);
 if(is(&m,"readback") || is(&m,"power-change"))assert(m.inspects==1);
 if(is(&m,"restore-fail"))assert(m.stores==2 && m.command==2);
 unsigned stores=m.stores,reads=m.inspects;struct intel_hda_owner_result copy=r;assert(!intel_hda_owner_trial(&r,f,io) && !intel_hda_owner_trial(&copy,f,io));assert(stores==m.stores && reads==m.inspects);
 puts("hda-owner-core: PASS cooperative inhibit/restore, strict idle/resources, deadline and retained uncertainty");return 0;}
