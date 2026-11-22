#ifndef SRVROS_XHCI_WIFI_RADIO_BANK_H
#define SRVROS_XHCI_WIFI_RADIO_BANK_H

/* The replacement adapter gets a separate RF owner after Disable Slot.
 * Standalone component fixtures exercise the original owner only. */
#ifdef SRVROS_XHCI_FULL_DRIVER
static unsigned wifi_radio5_bank;
#define WIFI_RADIO5_BANKS 2
#else
#define wifi_radio5_bank 0u
#define WIFI_RADIO5_BANKS 1
#endif

static inline unsigned wifi_radio5_generation(void) { return wifi_radio5_bank+1; }

#endif
