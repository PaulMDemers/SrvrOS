#ifndef SRVROS_USB_ECM_H
#define SRVROS_USB_ECM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct usb_ecm_endpoint { uint8_t address, interval, burst;uint16_t packet; };
struct usb_ecm_config {
    uint8_t configuration,control_interface,data_interface,data_alt,mac_string;
    uint16_t segment;
    struct usb_ecm_endpoint out,in,notify;
};
/* Initial profile: one ECM function, union descriptor, explicit data alternate,
 * normal Ethernet frames, full/high/super speed and no streams. */
bool usb_ecm_parse(const uint8_t *data,size_t size,unsigned speed,struct usb_ecm_config *out);
/* Exact captured-device compatibility; the generic parser remains strict. */
bool usb_ecm_parse_device(const uint8_t *data,size_t size,unsigned speed,uint16_t vendor,uint16_t product,uint16_t revision,struct usb_ecm_config *out);
bool usb_ecm_mac(const uint8_t *descriptor,size_t size,uint8_t mac[6]);
struct usb_ecm_link { bool known,up;uint32_t upstream_bps,downstream_bps; };
bool usb_ecm_notification(struct usb_ecm_link *link,const uint8_t *data,size_t size,uint8_t control_interface);
#endif
