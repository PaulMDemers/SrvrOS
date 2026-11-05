#include <srvros/intel_boot_claim.h>

static struct {
    unsigned policy,client;
    struct pci_device device;
} intel_boot_state;

static bool eligible(unsigned client,const struct pci_device *p) {
    return (client==INTEL_BOOT_LEGACY || client==INTEL_BOOT_OBSERVER) &&
        intel_boot_state.policy==client && p && p->vendor_id==0x8086 &&
        p->device_id && p->device_id!=0xffff && p->class_code==3 &&
        p->device<32 && p->function<8;
}
bool intel_boot_select(unsigned client) {
    if(intel_boot_state.policy || client<INTEL_BOOT_LEGACY || client>INTEL_BOOT_BLOCKED)return false;
    intel_boot_state.policy=client;
    return true;
}
bool intel_boot_check(unsigned client,const struct pci_device *p) {
    if(!eligible(client,p) || intel_boot_state.client!=client)return false;
    const struct pci_device *d=&intel_boot_state.device;
    if(d->bus!=p->bus || d->device!=p->device || d->function!=p->function ||
       d->vendor_id!=p->vendor_id || d->device_id!=p->device_id || d->revision!=p->revision ||
       d->class_code!=p->class_code || d->subclass!=p->subclass || d->prog_if!=p->prog_if ||
       d->header_type!=p->header_type)return false;
    for(unsigned i=0;i<6;i++)if(d->bar[i]!=p->bar[i])return false;
    return true;
}
bool intel_boot_claim(unsigned client,const struct pci_device *p) {
    if(!eligible(client,p))return false;
    if(intel_boot_state.client)return intel_boot_check(client,p);
    intel_boot_state.device=*p;
    intel_boot_state.client=client;
    return true;
}
unsigned intel_boot_policy(void){return intel_boot_state.policy;}
unsigned intel_boot_claimed_client(void){return intel_boot_state.client;}
