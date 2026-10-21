#include <srvros/mt76_bringup.h>
#include <srvros/sha256.h>
/* Only the pinned mt7610e candidate is admitted for this diagnostic. */
bool mt76_firmware_verified(const uint8_t *image,size_t size) {
 static const uint8_t expected[32]={0x53,0x8d,0x08,0xc0,0x8f,0x8f,0xb5,0xaa,0x05,0xca,0xcf,0x34,0xe4,0x9a,0xeb,0x30,0x7e,0x8b,0x2d,0xc0,0x94,0x40,0x7f,0xc6,0x55,0xfb,0xf3,0x58,0x0c,0x89,0x39,0x53};
 struct mt76_firmware_layout layout;
 if(!image || size!=80680 || !mt76_firmware_layout(image,size,&layout))return false;
 uint8_t digest[32];sha256_digest(image,size,digest);
 unsigned diff=0;for(unsigned i=0;i<32;i++)diff|=digest[i]^expected[i];return !diff;
}
