#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/arch/x86_64/syscall.c"
static void *allowed;
static size_t allowed_size;
static unsigned reads;
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    (void)writable;
    return address>=(uint64_t)allowed && address-(uint64_t)allowed<=allowed_size &&
        length<=allowed_size-(address-(uint64_t)allowed);
}
bool mouse_scan(struct mouse_event *event) {
    reads++; *event=(struct mouse_event){.dx=7,.dy=-8,.buttons=1,.flags=1,.wheel_x=-12,.wheel_y=48};
    return true;
}
int main(void) {
    _Static_assert(sizeof(struct syscall_mouse_event)==40,"extended mouse ABI");
    _Static_assert(offsetof(struct syscall_mouse_event,wheel_x)==32,"original prefix");
    struct { struct syscall_mouse_event event; unsigned char guard[8]; } buffer;
    memset(&buffer,0xa5,sizeof(buffer)); allowed=&buffer; allowed_size=32;
    buffer.event.abi_version=SRV_ABI_VERSION; buffer.event.struct_size=32;
    assert(syscall_mouse_scan(&buffer.event)==1 && buffer.event.dx==7 && buffer.event.dy==-8);
    for (size_t i=32;i<sizeof(buffer);i++) assert(((unsigned char *)&buffer)[i]==0xa5);
    buffer.event.struct_size=40; allowed_size=40;
    assert(syscall_mouse_scan(&buffer.event)==1 && buffer.event.wheel_x==-12 && buffer.event.wheel_y==48);
    for (unsigned i=0;i<8;i++) assert(buffer.guard[i]==0xa5);
    buffer.event.abi_version=999;
    assert(syscall_mouse_scan(&buffer.event)==-1 && reads==2);
    assert(syscall_mouse_scan(NULL)==-1 && reads==2);
    puts("mouse-abi: PASS legacy prefix, wheel fields, guard bytes, invalid request before dequeue");
}
