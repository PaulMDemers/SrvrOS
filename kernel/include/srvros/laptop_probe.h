#ifndef SRVROS_LAPTOP_PROBE_H
#define SRVROS_LAPTOP_PROBE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct laptop_resources { uint16_t status,data,pm1_control,gpe_enable;uint8_t gpe_mask; };
struct laptop_backlight { uint32_t duty,period;unsigned mode; }; /* 0 unsupported, 1 CPU, 2 PCH */
bool laptop_resources_parse(struct laptop_resources *out,const uint8_t *ecdt,size_t ec_size,const uint8_t *fadt,size_t fa_size);
bool laptop_firmware_match(const uint8_t *dsdt,size_t size);
struct laptop_backlight laptop_backlight_decode(uint32_t cpu_control,uint32_t cpu_duty,uint32_t pch_control,uint32_t pch_duty);
void laptop_probe(void); /* Read-only, opt-in; no EC commands, AML or PWM writes. */
void laptop_brightness_init(void); /* Opt-in provider discovery; no brightness write. */
#endif
