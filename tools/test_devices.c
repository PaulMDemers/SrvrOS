#include <assert.h>
#include <stdio.h>
#include <srvros/devices.h>
#include <srvros/pci.h>
static const struct pci_device devices[]={ {.bus=0,.device=27,.vendor_id=0x8086,.device_id=0x9ca0,.class_code=4,.subclass=3},
    {.bus=2,.device=1,.function=2,.vendor_id=0x14e4,.device_id=0x43a0,.class_code=2,.subclass=128} };
uint64_t pci_device_count(void) { return 2; }
const struct pci_device *pci_device_at(uint64_t i) { return i<2?&devices[i]:0; }
int64_t xhci_device_snapshot(uint64_t i,struct srv_device_info *out) {
    if(i>2) return 0;
    out->index=2;out->vendor_id=0x05ac;return 3;
}
int main(void) {
    struct srv_device_info d;
    assert(device_snapshot(SRV_DEVICE_PCI,0,&d)==1&&d.version==1&&d.size==80&&d.vendor_id==0x8086&&d.device==27);
    assert(device_snapshot(SRV_DEVICE_PCI,1,&d)==2&&d.bus==2&&d.function==2&&d.flags==SRV_DEVICE_PRESENT);
    assert(device_snapshot(SRV_DEVICE_PCI,2,&d)==0&&d.version==1&&!d.vendor_id);
    assert(device_snapshot(SRV_DEVICE_PCI,UINT64_MAX,&d)==0);
    assert(device_snapshot(SRV_DEVICE_USB,0,&d)==3&&d.bus_kind==SRV_DEVICE_USB&&d.index==2);
    assert(device_snapshot(0,0,&d)==-1&&device_snapshot(1,0,0)==-1);
    puts("devices: cached PCI fields, sparse USB indexes, ABI initialization and bounds passed");
}
