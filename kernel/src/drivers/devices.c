#include <srvros/devices.h>
#include <srvros/pci.h>
#include <srvros/xhci.h>
int64_t device_snapshot(uint64_t kind,uint64_t index,struct srv_device_info *out) {
    if(!out || (kind!=SRV_DEVICE_PCI && kind!=SRV_DEVICE_USB)) return -1;
    *out=(struct srv_device_info){.version=SRV_DEVICE_VERSION,.size=sizeof(*out),.bus_kind=kind};
    if(kind==SRV_DEVICE_USB) return xhci_device_snapshot(index,out);
    if(index>=pci_device_count()) return 0;
    const struct pci_device *p=pci_device_at(index);
    if(!p) return -1;
    out->index=index;out->vendor_id=p->vendor_id;out->product_id=p->device_id;
    out->class_code=p->class_code;out->subclass=p->subclass;out->protocol=p->prog_if;out->revision=p->revision;
    out->bus=p->bus;out->device=p->device;out->function=p->function;out->flags=SRV_DEVICE_PRESENT;
    return (int64_t)index+1;
}
