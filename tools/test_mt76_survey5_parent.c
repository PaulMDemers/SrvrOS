/* Compose the actual RF inverse engine with the actual native survey callback
 * in the other test translation unit. Registers/HCD remain independent models. */
#define MT76_CHANNEL_VCO_TEST_MAIN unused_survey_rf_main
#include "test_mt76_channel_vco5.c"
#undef MT76_CHANNEL_VCO_TEST_MAIN
static bool (*survey_native_callback)(void);
static unsigned survey_native_calls;
static bool survey_inspect_selected(void *p){
 struct fake *f=p;assert(++survey_native_calls==1 && f->calls==2636);
 assert(mt76_channel_vco5_selected(&combo) && mt76_cvco5_valid(&combo.vco));
 selected_bank(f,3);return survey_native_callback();
}
bool survey5_test_RF_parent(bool (*callback)(void),bool closure_expected){
 struct fake f={0};combo_init(&f);clear_at=4;survey_native_callback=callback;survey_native_calls=0;
 const struct mt76_rf_wait_io rf={{&f,combo_auth,combo_read,combo_exchange},combo_delay};
 const struct mt76_band_mac_io mac={&f,combo_auth,combo_mac};
 const struct mt76_pa5_io pa={&f,combo_auth,combo_direct,combo_exchange};
 const struct mt76_bbp5_io bbp={&f,combo_auth,combo_direct};
 enum mt76_mcu_timing_status status=mt76_channel_vco5_inspect(&combo,0xfd11,0x1004,0x44,12,&rf,&mac,&pa,&bbp,survey_inspect_selected);
 assert(survey_native_calls==1);
 if(closure_expected){
  assert(status==MT76_TIMING_OK && mt76_channel_vco5_valid(&combo) &&
   combo.restored && f.calls==3844 && !f.rf[0][22] && f.rf[0][4]==0x30);
 }else assert(status!=MT76_TIMING_OK && !combo.complete && f.calls==2636 && f.rf[0][22]==0x44);
 return status==MT76_TIMING_OK;
}
