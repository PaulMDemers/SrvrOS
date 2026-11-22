/* Host-only ABI sizes for the independent ctypes differential harness. */
#include <srvros/wpa2_handshake.h>
size_t wpa2_probe_state_size(void){return sizeof(struct wpa2_handshake);}
size_t wpa2_probe_peer_size(void){return sizeof(struct wpa2_hs_peer);}
size_t wpa2_probe_keys_size(void){return sizeof(struct wpa2_hs_keys);}
#include <srvros/wpa2_ccmp.h>
size_t wpa2_probe_ccmp_size(void){return sizeof(struct wpa2_ccmp);}
