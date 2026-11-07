#ifndef SRVROS_MT76_RSN_H
#define SRVROS_MT76_RSN_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Advertisement validation only: not proof of AP identity/authentication.
 * Initial profile requires explicit CCMP group+pairwise and PSK AKM. No WPA1,
 * TKIP, enterprise, SAE-only, PMF-required, cached PMKID or silent defaults. */
enum mt76_rsn_status {MT76_RSN_OK,MT76_RSN_INVALID,MT76_RSN_UNSUPPORTED};
struct mt76_rsn_profile {uint16_t capabilities;bool pmf_capable;};
enum mt76_rsn_status mt76_rsn_psk_ccmp(const uint8_t *,size_t,struct mt76_rsn_profile *);
/* Minimal explicit station RSNE for this profile, no PMKID or PMF assertion. */
size_t mt76_rsn_station_ie(uint8_t *,size_t);
#endif
