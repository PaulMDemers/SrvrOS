#ifndef SRVROS_HDA_CODEC_H
#define SRVROS_HDA_CODEC_H
#include <srvros/hda_core.h>
enum hda_output { HDA_SPEAKERS=0, HDA_HEADPHONES=1 };
enum hda_profile { HDA_GENERIC, HDA_AIR7_2, HDA_PRO11_2 };
struct hda_codec {
    struct hda_core *controller;
    uint32_t vendor, subsystem, gpio_mask, gpio_direction, gpio_data;
    uint8_t address, afg, routes, pin[3], dac[3];
    uint32_t dac_caps[3], pin_caps[3], dac_amp[3], pin_amp[3];
    bool apple, available, muted;
    enum hda_output output;
};
bool hda_codec_discover(struct hda_codec *,struct hda_core *,enum hda_profile profile);
bool hda_codec_route(struct hda_codec *,enum hda_output);
bool hda_codec_mute(struct hda_codec *,bool mute);
bool hda_codec_headphones(struct hda_codec *,bool *present);
#endif
