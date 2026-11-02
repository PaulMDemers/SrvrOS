/* Linux sysfs adapter for the exact existing diagnostic PCI decoder.
 * O_RDONLY/pread only; no BAR probing, MMIO or configuration-data writes. */
#define _POSIX_C_SOURCE 200809L
#include <srvros/package_audit.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
struct source {int fd;bool failed;};
static bool read32(void *ctx,unsigned offset,uint32_t *out) {
    struct source *s=ctx;unsigned char b[4];
    if(offset>0xffc || (offset&3) || pread(s->fd,b,4,offset)!=4) {
        s->failed=true;return false;
    }
    *out=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
    return true;
}
static uint32_t conventional(void *ctx,unsigned offset) {
    uint32_t v=0xffffffffu;
    if(offset>0xfc){((struct source *)ctx)->failed=true;return v;}
    (void)read32(ctx,offset,&v);return v;
}
int main(int argc,char **argv) {
    if(argc!=2){fprintf(stderr,"usage: linux-package-pci CONFIG_FILE\n");return 2;}
    struct source s={.fd=open(argv[1],O_RDONLY)};
    if(s.fd<0){perror("open config");return 1;}
    struct package_pci_snapshot p;struct package_ltr l={.state=2};
    package_pci_read(&p,conventional,&s);bool conventional_failed=s.failed;
    s.failed=false;
    if(p.state==1 && !conventional_failed && (p.flags&6))package_ltr_read(&l,read32,&s);
    printf("{\"state\":%u,\"read_error\":%s,\"id\":%u,\"class\":%u,\"command\":%u,\"flags\":%u,\"pm\":%u,\"link_cap\":%u,\"link\":%u,\"ltr_cap\":%u,\"ltr\":%u,\"extended\":{\"state\":%u,\"read_error\":%s,\"offset\":%u,\"raw\":%u,\"headers\":%u}}\n",
           p.state,conventional_failed?"true":"false",p.id,p.class_code,p.command,p.flags,p.pm,p.link_cap,p.link,p.ltr_cap,p.ltr,
           l.state,s.failed?"true":"false",l.offset,l.raw,l.headers);
    close(s.fd);return 0;
}
