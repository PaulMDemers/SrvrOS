#include <srvros/hda_codec.h>

static bool verb(struct hda_codec *c,unsigned node,unsigned op,unsigned value,uint32_t *out) {
    uint32_t ignored;
    return hda_core_verb(c->controller,c->address,node,op,value,out?out:&ignored);
}
static bool parameter(struct hda_codec *c,unsigned node,unsigned id,uint32_t *out) {
    return verb(c,node,0xf00,id,out);
}
static bool power_on(struct hda_codec *c,unsigned node) {
    if(!verb(c,node,0x705,0,0)) return false;
    for(unsigned attempt=0;attempt<20;attempt++) {
        uint32_t state;
        if(!verb(c,node,0xf05,0,&state)) return false;
        if(state&0x100) { c->controller->error=HDA_POWER;return false; }
        if(!(state&0xff)) return true;
        if(!hda_core_delay(c->controller,5000)) return false;
    }
    c->controller->error=HDA_POWER;return false;
}
static bool amp(struct hda_codec *c,unsigned node,uint32_t caps,uint32_t acaps,bool mute) {
    if(!(caps&4)) return true;
    unsigned unity=acaps&127, steps=(acaps>>8)&127;
    if(unity>steps) return false;
    return verb(c,node,0x300,0xb000|unity|(mute?128:0),0);
}
static bool validate_route(struct hda_codec *c,unsigned r) {
    uint32_t pincap,connections,pcm,formats;
    if(!parameter(c,c->pin[r],9,&c->pin_caps[r]) ||
       !parameter(c,c->dac[r],9,&c->dac_caps[r]) ||
       !parameter(c,c->pin[r],12,&pincap) ||
       !parameter(c,c->pin[r],14,&connections) || connections!=1 ||
       !verb(c,c->pin[r],0xf02,0,&connections) || connections!=c->dac[r] ||
       ((c->pin_caps[r]>>20)&15)!=4 || ((c->dac_caps[r]>>20)&15)!=0 ||
       !(c->dac_caps[r]&1) || !(pincap&16)) return false;
    unsigned format_node=(c->dac_caps[r]&16)?c->dac[r]:c->afg;
    if(!parameter(c,format_node,10,&pcm)||!parameter(c,format_node,11,&formats) ||
       !(pcm&(1<<6)) || !(pcm&(1<<17)) || !(formats&1)) return false;
    if((c->dac_caps[r]&4) && !parameter(c,(c->dac_caps[r]&8)?c->dac[r]:c->afg,18,&c->dac_amp[r])) return false;
    if((c->pin_caps[r]&4) && !parameter(c,(c->pin_caps[r]&8)?c->pin[r]:c->afg,18,&c->pin_amp[r])) return false;
    return true;
}
bool hda_codec_discover(struct hda_codec *c,struct hda_core *h,enum hda_profile profile) {
    if(!c||!h||!h->ready) return false;
    *c=(struct hda_codec){.controller=h};
    for(unsigned address=0;address<15;address++) {
        if(!(h->codecs&(1u<<address))) continue;
        c->address=address;
        uint32_t groups,type;
        if(!parameter(c,0,0,&c->vendor)||!parameter(c,0,4,&groups)) return false;
        /* Initial supported codecs have one AFG, node 1. Unsupported hardware
         * stays untouched: don't guess a speaker GPIO or an analog route. */
        if(groups!=0x10001 || !parameter(c,1,5,&type)||(type&255)!=1) continue;
        c->afg=1;
        if(!verb(c,1,0xf20,0,&c->subsystem)) return false;
        bool pro=profile==HDA_PRO11_2 && c->vendor==0x10134208 && c->subsystem==0x106b5e00;
        c->apple=pro || (profile==HDA_AIR7_2 && c->vendor==0x10134208 && c->subsystem==0x106b7e00);
        bool qemu=profile==HDA_GENERIC && (c->vendor==0x1af40012||c->vendor==0x1af40022||c->vendor==0x1af40032);
        if(!c->apple&&!qemu) continue;
        c->pin[0]=c->apple?0x12:3; c->dac[0]=c->apple?3:2;
        c->pin[1]=0x10; c->dac[1]=2;
        c->routes=pro?3:c->apple?2:1;
        c->pin[2]=0x13;c->dac[2]=4;
        if(pro) {
            uint32_t value;
            if(!parameter(c,0,2,&value)||value!=0x100300) return false;
            const uint32_t defaults[]={0x90100110,0x002b4020,0x90100112};
            for(unsigned r=0;r<3;r++)
                if(!verb(c,c->pin[r],0xf1c,0,&value)||value!=defaults[r]) return false;
        }
        for(unsigned r=0;r<c->routes;r++) if(!validate_route(c,r)) return false;
        if(c->apple) {
            /* CS4208 A1 coefficient setup, gated by the observed codec + board
             * subsystem. Register values are documented in the audio bring-up
             * note; only the two explicitly matched boards use these coefficients. */
            if(!power_on(c,1)||!verb(c,0x24,0x703,1,0)||
               !verb(c,0x24,0x500,0x33,0)||!verb(c,0x24,0x400,1,0)||
               !verb(c,0x24,0x500,0x34,0)||!verb(c,0x24,0x400,0x1c01,0)||
               !verb(c,1,0xf16,0,&c->gpio_mask)||!verb(c,1,0xf17,0,&c->gpio_direction)||
               !verb(c,1,0xf15,0,&c->gpio_data)) return false;
            if(!verb(c,1,0x716,c->gpio_mask|1,0)||
               !verb(c,1,0x717,c->gpio_direction|1,0)) return false;
        }
        c->available=true;
        return hda_codec_route(c,HDA_SPEAKERS);
    }
    return false;
}
bool hda_codec_route(struct hda_codec *c,enum hda_output output) {
    if(!c||!c->available||output>HDA_HEADPHONES || (output==HDA_HEADPHONES&&!c->apple)) return false;
    if(c->controller->playing) return false;
    for(unsigned r=0;r<c->routes;r++) {
        bool active=(r==1)==(output==HDA_HEADPHONES);
        uint32_t caps=c->dac_caps[r];
        if((caps&(1<<10))&&!power_on(c,c->dac[r])) goto fail;
        if((c->pin_caps[r]&(1<<10))&&!power_on(c,c->pin[r])) goto fail;
        if(!amp(c,c->dac[r],caps,c->dac_amp[r],true) ||
           !amp(c,c->pin[r],c->pin_caps[r],c->pin_amp[r],true) ||
           !verb(c,c->dac[r],0x200,0x11,0)||
           !verb(c,c->dac[r],0x706,active?0x10:0,0)||
           !verb(c,c->pin[r],0x707,active?(r==1?0xc0:0x40):0,0)) goto fail;
    }
    if(c->apple&&!verb(c,1,0x715,(c->gpio_data&~1u)|(output==HDA_SPEAKERS),0)) goto fail;
    c->output=output;c->muted=true; return true;
fail:
    c->available=false; return false;
}
bool hda_codec_mute(struct hda_codec *c,bool mute) {
    if(!c||!c->available) return false;
    for(unsigned r=0;r<c->routes;r++) {
        if((r==1)!=(c->output==HDA_HEADPHONES)) continue;
        if(!amp(c,c->dac[r],c->dac_caps[r],c->dac_amp[r],mute)||
           !amp(c,c->pin[r],c->pin_caps[r],c->pin_amp[r],mute)) { c->available=false;return false; }
    }
    c->muted=mute;return true;
}
bool hda_codec_headphones(struct hda_codec *c,bool *present) {
    uint32_t sense;
    if(!c||!c->available||!c->apple||!present||!verb(c,0x10,0xf09,0,&sense)) return false;
    *present=(sense>>31)!=0; return true;
}
