#ifndef SRVROS_SBS_SAMPLE_H
#define SRVROS_SBS_SAMPLE_H
#include <srvros/ec_smbus.h>
/* Read-only Smart Battery Data sampler. There is deliberately no native
 * binding in this module. The caller must establish EC/SMBus ownership and battery selection,
 * serialize this transport with AML/query work, and schedule polls. */
#define SBS_WORD_COUNT 15
#define SBS_VALID_SOC 1u
#define SBS_VALID_VOLTAGE 2u
#define SBS_VALID_CURRENT 4u
#define SBS_VALID_CAPACITY 8u
#define SBS_VALID_TEMPERATURE 16u
#define SBS_VALID_TIME_EMPTY 32u
#define SBS_VALID_TIME_FULL 64u
#define SBS_CAPACITY_UAH 1u
#define SBS_CAPACITY_UWH 2u
struct sbs_snapshot {
    uint64_t started_us,completed_us,generation;
    uint64_t voltage_uv,remaining_capacity,full_capacity,design_capacity;
    int64_t current_ua;
    int32_t temperature_mc;
    uint32_t valid,capacity_unit;
    uint16_t specification,mode,status,relative_soc,minutes_empty,minutes_full;
    uint16_t raw[SBS_WORD_COUNT];
};
enum sbs_error { SBS_OK,SBS_TRANSPORT,SBS_CLOCK,SBS_DEADLINE,SBS_POLL_LIMIT,SBS_CHANGED_MODE,SBS_CANCELED };
struct sbs_sampler {
    struct ec_smbus transaction;
    struct ec_transport *ec;
    struct sbs_snapshot sample,staging;
    uint64_t last_us;
    uint32_t polls;
    unsigned index;
    uint8_t base;
    bool active,waiting,current;
    enum sbs_error error;
};
/* Zero initialize first. Failed samples retain the last successful sample and
 * timestamp, but clear current. No battery/charger writes are ever requested. */
bool sbs_sample_begin(struct sbs_sampler *,struct ec_transport *,uint8_t base,uint64_t now_us);
int sbs_sample_poll(struct sbs_sampler *,uint64_t now_us);
void sbs_sample_cancel(struct sbs_sampler *);
bool sbs_sample_fresh(const struct sbs_sampler *,uint64_t now_us,uint64_t max_age_us);
/* Pure decoder preserves every raw word. Invalid spec/scales/SOC/time fields
 * are not silently clamped; only supported fields receive validity bits. */
void sbs_sample_decode(struct sbs_snapshot *);
#endif
