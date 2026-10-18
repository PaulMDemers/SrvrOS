#ifndef SRVROS_HDA_CODEC_H
#define SRVROS_HDA_CODEC_H
#include <srvros/hda_core.h>
enum hda_output { HDA_SPEAKERS=0, HDA_HEADPHONES=1 };
struct hda_codec {
    struct hda_core *controller;
    uint32_t vendor, subsystem, gpio_mask, gpio_direction, gpio_data;
    uint8_t address, afg, pin[2], dac[2];
    uint32_t dac_caps[2], pin_caps[2], dac_amp[2], pin_amp[2];
    bool apple, available, muted;
    enum hda_output output;
};
bool hda_codec_discover(struct hda_codec *,struct hda_core *,bool apple_controller);
bool hda_codec_route(struct hda_codec *,enum hda_output);
bool hda_codec_mute(struct hda_codec *,bool mute);
bool hda_codec_headphones(struct hda_codec *,bool *present);
#endif
