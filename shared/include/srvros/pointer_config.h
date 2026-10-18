#ifndef SRVROS_POINTER_CONFIG_H
#define SRVROS_POINTER_CONFIG_H
/* SYS_GUI_POINTER_CONFIG: speed 1..5 and these flags. Only the compositor may
 * change them. Zero leaves typing and trackpad movement independent. */
#define SRV_POINTER_ACCELERATION 1u
#define SRV_POINTER_DISABLE_WHILE_TYPING 2u
#define SRV_POINTER_TAP_CLICK 4u
#define SRV_POINTER_TWO_FINGER_TAP 8u
#define SRV_POINTER_TAPS (SRV_POINTER_TAP_CLICK|SRV_POINTER_TWO_FINGER_TAP)
#define SRV_POINTER_FLAGS 15u
#endif
