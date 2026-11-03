#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <srvros/initramfs.h>
void console_write(const char *s) {(void)s;}
void console_printf(const char *s, ...) {(void)s;}
int main(void) {
    unsigned char a[1024] = {0};
    memcpy(a, "./", 2); memset(a+2, 'x', 98); memset(a+100, '7', 8);
    memcpy(a+124, "00000000001", 11);
    initramfs_init(a, 512); assert(initramfs_file_count()==0);
    initramfs_init(a, sizeof(a)); assert(initramfs_file_count()==1);
    const struct initramfs_file *f=initramfs_file_at(0);
    assert(f->size==1 && strlen(f->path)==99);
    return 0;
}
