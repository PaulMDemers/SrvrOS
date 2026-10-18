#ifndef SRVROS_HDA_CORE_H
#define SRVROS_HDA_CORE_H
#include <stdbool.h>
#include <stdint.h>

/* Register access is deliberately width-specific: SDSTS shares a dword with
 * SDCTL, and writing its W1C bits as part of a control update loses errors. */
struct hda_io {
    void *context;
    uint32_t (*read)(void *, unsigned offset, unsigned bytes);
    void (*write)(void *, unsigned offset, unsigned bytes, uint32_t value);
    uint64_t (*now_us)(void *);
    void (*relax)(void *);
};
enum hda_error { HDA_OK, HDA_BAD_ARGUMENT, HDA_RESET_TIMEOUT, HDA_COMMAND_TIMEOUT,
    HDA_RESPONSE_MISMATCH, HDA_STREAM_TIMEOUT, HDA_STREAM_ERROR, HDA_UNSUPPORTED_CODEC,
    HDA_DMA_MEMORY, HDA_WORKER, HDA_MAPPING, HDA_BAD_BAR, HDA_POWER };
struct hda_core {
    struct hda_io io;
    enum hda_error error;
    uint16_t codecs, stream;
    uint8_t inputs, outputs;
    bool dma64, ready, playing;
    uint64_t commands;
    uint32_t last_command, last_response, last_position;
};
bool hda_core_reset(struct hda_core *, struct hda_io);
bool hda_core_delay(struct hda_core *,unsigned microseconds);
bool hda_core_verb(struct hda_core *, unsigned codec, unsigned node,
    unsigned verb, unsigned payload, uint32_t *response);
bool hda_core_start(struct hda_core *, uint64_t bdl, uint32_t bytes, unsigned entries);
bool hda_core_stop(struct hda_core *);
bool hda_core_position(struct hda_core *, uint32_t *position);
const char *hda_error_name(enum hda_error);
#endif
