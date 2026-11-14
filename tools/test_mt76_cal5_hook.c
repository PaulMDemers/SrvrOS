#define MT76_CHANNEL_VCO_TEST_MAIN legacy_channel_vco_main
#include "test_mt76_channel_vco5.c"
#undef MT76_CHANNEL_VCO_TEST_MAIN
static unsigned inspected,damage;
static bool inspect_selected(void *p){
 struct fake *f=p;inspected++;assert(inspected==1 && f->calls==2636 && waits==6);
 assert(mt76_channel_vco5_selected(&combo) && mt76_cvco5_valid(&combo.vco));
 selected_bank(f,3);assert(f->rf[0][22]==0x44 && f->rf[0][4]==0x30);
 assert(combo_run(f,0x44,12)==MT76_TIMING_REFUSED);
 if(damage==1)return false;
 if(damage==2)combo.factory_trim^=1;
 if(damage==3)combo.trim.channel.bbp.operations++;
 if(damage==4)combo.vco.complete=false;
 return true;
}
static enum mt76_mcu_timing_status inspected_run(struct fake *f,bool present){
 const struct mt76_rf_wait_io rf={{f,combo_auth,combo_read,combo_exchange},combo_delay};
 const struct mt76_band_mac_io mac={f,combo_auth,combo_mac};
 const struct mt76_pa5_io pa={f,combo_auth,combo_direct,combo_exchange};
 const struct mt76_bbp5_io bbp={f,combo_auth,combo_direct};
 return mt76_channel_vco5_inspect(&combo,0xfd11,0x1004,0x44,12,&rf,&mac,&pa,&bbp,present?inspect_selected:NULL);
}
int main(void){
 struct fake f={0};
 for(unsigned n=0;n<5;n++){
  damage=n;inspected=0;combo_init(&f);clear_at=4;
  enum mt76_mcu_timing_status status=inspected_run(&f,true);
  assert(inspected==1);
  if(!n)assert(status==MT76_TIMING_OK && mt76_channel_vco5_valid(&combo) && f.calls==3844 && !f.rf[0][22]);
  else assert(status!=MT76_TIMING_OK && !combo.complete && f.calls==2636 && f.rf[0][22]==0x44);
  assert(inspected_run(&f,true)==MT76_TIMING_REFUSED && inspected==1);
 }
 inspected=0;damage=0;combo_init(&f);assert(inspected_run(&f,false)==MT76_TIMING_REFUSED && !f.calls && !combo.attempted);
 puts("cal5-hook: PASS exact selected checkpoint after VCO clear,before inverse,one-shot/reentrant/null/refusal and parent/proof mutation;unchanged no-hook legacy plan");
 return 0;
}
