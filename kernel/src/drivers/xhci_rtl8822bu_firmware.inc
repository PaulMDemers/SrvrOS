/* Netgear A6150 digital-only firmware download over its USB2 bulk OUT pipe.
 * Bound to the verified firmware 30.20.0 and the independently captured
 * port-4 descriptor. The staging queue never enables MAC TX or baseband/RF. */
enum {
    BU_OUT_DCI=10, BU_DESC_BYTES=48, BU_BLOCK_BYTES=4096,
    BU_FW_BYTES=161240, BU_FW_HEADER=64, BU_DMEM_BYTES=11216, BU_IMEM_BYTES=149960,
    BU_MCU=0x0080, BU_CR=0x0100, BU_BCN_HEAD=0x0204, BU_BCN=0x0550,
    BU_DDMA_SRC=0x1200, BU_DDMA_DST=0x1204, BU_DDMA=0x1208,
    BU_DDMA_OWN=1u<<31, BU_DDMA_CHECK=1u<<29, BU_DDMA_BAD=1u<<27,
    BU_DDMA_RESET=1u<<25, BU_DDMA_MORE=1u<<24, BU_MCU_READY=0xc078,
    BU_MCU_DOWNLOAD=1u, BU_MCU_BUSY=0xc001, BU_MCU_FLASH=1u<<20,
    BU_MCU_COLD_LATCH=0x00600001, BU_MCU_PLATFORM_STATUS=1u<<31, BU_GPIO_FLASH=1u<<19,
    BU_WLRF1=0x00ec, BU_WLRF1_BBRF=7u<<24,
};

struct bu_upload {
    struct xhci_device *device,*hub;
    struct xhci_bulk_endpoint out;
    uint8_t *packet;
    uint64_t packet_phys;
    unsigned bytes,packets;
    uint32_t mcu;
    bool power_touched,unexpected_mcu,endpoint_ready,stopped,parked;
};

static bool bu_image_valid(const uint8_t *data,uint64_t size) {
    static const uint8_t expected[32]={
        0xa7,0x2d,0xa6,0x90,0x59,0x7b,0xfa,0x99,0xd8,0xeb,0x6f,0xc2,0xab,0x09,0x0d,0x18,
        0xd8,0xad,0x92,0xac,0x2b,0xef,0xd3,0x5d,0xb1,0xc9,0xe3,0x66,0x2d,0x8d,0x84,0x18,
    };
    _Static_assert(BU_FW_HEADER+BU_DMEM_BYTES+BU_IMEM_BYTES==BU_FW_BYTES,"firmware sections");
    uint8_t actual[32];
    if(!data || size!=BU_FW_BYTES)return false;
    sha256_digest(data,size,actual);
    return bytes_equal(actual,expected,sizeof expected);
}

static bool bu_io(struct xhci_device *d,bool in,uint16_t reg,uint16_t width,uint32_t *value) {
    if(!in)for(unsigned i=0;i<width;i++)d->descriptor_buffer[i]=(uint8_t)(*value>>(8*i));
    const struct usb_setup_packet setup={.request_type=in?0xc0:0x40,.request=5,.value=reg,.length=width};
    int actual;
    if(!control_transfer_internal(d,&setup,d->descriptor_buffer_phys,width,in,&actual) || actual!=width) {
        console_printf("rtl8822bu-control: reg=%x width=%u in=%u failed=1\n",
            (uint64_t)reg,(uint64_t)width,(uint64_t)in);
        return false;
    }
    if(in) {
        *value=0;
        for(unsigned i=0;i<width;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    }
    /* The always-on register bank uses a follow-up byte write after a
     * transfer. Confirmed on the wire for both directions in Ubuntu. */
    if(reg<=0xff || (reg>=0x1000 && reg<=0x10ff)) {
        const struct usb_setup_packet echo={.request_type=0x40,.request=5,.value=0x04e0,.length=1};
        d->descriptor_buffer[0]=(uint8_t)*value;
        if(!control_transfer_internal(d,&echo,d->descriptor_buffer_phys,1,false,&actual) || actual!=1) {
            console_printf("rtl8822bu-control: reg=%x width=%u echo_failed=1\n",(uint64_t)reg,(uint64_t)width);
            return false;
        }
    }
    return true;
}

static bool bu_read(struct xhci_device *d,uint16_t reg,uint16_t width,uint32_t *value) {
    return bu_io(d,true,reg,width,value);
}
static bool bu_write(struct xhci_device *d,uint16_t reg,uint16_t width,uint32_t value) {
    return bu_io(d,false,reg,width,&value);
}
static bool bu_update(struct xhci_device *d,uint16_t reg,uint16_t width,uint32_t mask,uint32_t bits) {
    uint32_t value;
    return bu_read(d,reg,width,&value) && bu_write(d,reg,width,(value&~mask)|bits);
}
static bool bu_poll(struct xhci_device *d,uint16_t reg,uint16_t width,uint32_t mask,uint32_t expected,
                    unsigned attempts,uint32_t *last) {
    for(unsigned i=0;i<attempts;i++) {
        if(!bu_read(d,reg,width,last))return false;
        if((*last&mask)==expected)return true;
        if(!timer_delay_us(50))return false;
    }
    console_printf("rtl8822bu-poll: reg=%x expected=%x mask=%x last=%x\n",
        (uint64_t)reg,(uint64_t)expected,(uint64_t)mask,(uint64_t)*last);
    return false;
}

static bool bu_radio_off(struct xhci_device *d) {
    uint32_t a,b,c;
    if(!bu_read(d,0x02,1,&a) || !bu_read(d,0x1f,1,&b) || !bu_read(d,BU_WLRF1,4,&c))return false;
    bool parked=!(a&3) && !(b&7) && !(c&BU_WLRF1_BBRF);
    console_printf("rtl8822bu-radio-gate: sys_func=%x rf_ctrl=%x wlrf1=%x parked=%u\n",
        (uint64_t)a,(uint64_t)b,(uint64_t)c,(uint64_t)parked);
    return parked;
}

static bool bu_clear_cold_latch(struct bu_upload *up,uint32_t mcu) {
    struct xhci_device *d=up->device;
    uint32_t cpu,io,cr,ddma,after;
    if(!wifi_hub_check_port(d,up->hub) || !bu_read(d,3,1,&cpu) || !bu_read(d,0x1d,1,&io) ||
       !bu_read(d,BU_CR,1,&cr) || !bu_read(d,BU_DDMA,4,&ddma))return false;
    console_printf("rtl8822bu-power-cold-latch: cpu=%x io=%x cr=%x ddma=%x\n",
        (uint64_t)cpu,(uint64_t)io,(uint64_t)cr,(uint64_t)ddma);
    if((cpu&4) || (io&1) || cr || (ddma&BU_DDMA_OWN) || !bu_radio_off(d))return false;
    if(!bu_write(d,BU_MCU,1,mcu&0xfeu) || !bu_read(d,BU_MCU,4,&after))return false;
    bool cleared=(after&~BU_MCU_PLATFORM_STATUS)==(BU_MCU_COLD_LATCH&~BU_MCU_DOWNLOAD);
    console_printf("rtl8822bu-power-cold-latch-result: mcu=%x cleared=%u\n",(uint64_t)after,(uint64_t)cleared);
    return cleared;
}

static const char *bu_power_on(struct bu_upload *up) {
    struct xhci_device *d=up->device;
    uint32_t value,status,mcu;
    if(!bu_read(d,BU_CR,1,&value) || !bu_read(d,0x00f5,1,&status) || !bu_read(d,BU_MCU,4,&mcu))
        return "entry-read";
    console_printf("rtl8822bu-power-entry: cr=%x sys_status=%x mcu=%x\n",
        (uint64_t)value,(uint64_t)status,(uint64_t)mcu);
    const bool cold_latch=value==0xea && status==0x10 && mcu==BU_MCU_COLD_LATCH;
    if((value!=0xea && !(status&1)) || ((mcu&BU_MCU_BUSY) && !cold_latch))return "entry-owner";
    /* Until the CPU and DMA are checked, even the observed cold signature
     * grants no authority to clear the latch on the failure path. */
    up->unexpected_mcu=cold_latch;
    up->power_touched=true;
    if(!bu_write(d,0x001c,1,0) || !bu_update(d,0x0064,4,0,0x30000000u) ||
       !bu_update(d,0x004c,4,0x06000000u,0) || !bu_update(d,0x0040,4,0,4) ||
       !bu_update(d,0x0002,1,3,0) || !bu_update(d,0x001f,1,7,0) ||
       !bu_update(d,BU_WLRF1,4,BU_WLRF1_BBRF,0))return "prepare";

    /* USB card-disabled -> emulation -> active. Cut 3 skips the cut-C-only
     * rows. The delay and the two polls are barriers between register writes. */
    static const struct { uint16_t reg; uint8_t clear,bits; } before_ready[]={
        {0x4a,1,0},{0x05,0x98,0},{0x12,2,0},{0x12,1,1},{0x20,1,1},
    }, before_active[]={
        {0xff1a,0xff,0},{0x06,1,1},{0x05,0x80,0},{0x05,0x18,0},{0x10c3,1,1},{0x05,1,1},
    }, after_active[]={
        {0x20,8,8},{0x29,0xff,0xf9},{0x24,4,0},{0xaf,0x20,0x20},{0xf5,1,0},
    };
    for(unsigned i=0;i<sizeof before_ready/sizeof before_ready[0];i++)
        if(!bu_update(d,before_ready[i].reg,1,before_ready[i].clear,before_ready[i].bits))return "emulation";
    if(!timer_delay_us(1000) || !bu_update(d,0,1,0x20,0) || !bu_update(d,5,1,0x1c,0) ||
       !bu_poll(d,6,1,2,2,100,&value))return "power-ready";
    for(unsigned i=0;i<sizeof before_active/sizeof before_active[0];i++)
        if(!bu_update(d,before_active[i].reg,1,before_active[i].clear,before_active[i].bits))return "activate";
    if(!bu_poll(d,5,1,1,0,100,&value))return "active-ready";
    for(unsigned i=0;i<sizeof after_active/sizeof after_active[0];i++)
        if(!bu_update(d,after_active[i].reg,1,after_active[i].clear,after_active[i].bits))return "active-config";
    if(!bu_update(d,3,1,4,0) || (cold_latch && !bu_update(d,0x1d,1,1,0)) ||
       !bu_update(d,0x1080,4,0,0x10100) || !bu_update(d,0x1103,1,15,12) ||
       !bu_read(d,BU_MCU,4,&mcu))return "platform";
    console_printf("rtl8822bu-power-platform: mcu=%x\n",(uint64_t)mcu);
    up->unexpected_mcu=(mcu&BU_MCU_BUSY)!=0;
    if(up->unexpected_mcu) {
        if(!cold_latch || (mcu&~BU_MCU_PLATFORM_STATUS)!=BU_MCU_COLD_LATCH)return "platform-owner";
        if(!bu_clear_cold_latch(up,mcu))return "cold-latch";
        mcu&=~BU_MCU_DOWNLOAD;
        up->unexpected_mcu=false;
    }
    /* Select host download before enabling the CPU interface. Do not touch
     * flash itself. Preserve the unrelated GPIO mux and MCU control bits. */
    if(mcu&BU_MCU_FLASH) {
        if(!bu_write(d,BU_MCU,4,mcu&~BU_MCU_FLASH) || !bu_update(d,0x40,4,BU_GPIO_FLASH,0) ||
           !bu_read(d,BU_MCU,4,&mcu))return "flash-select";
        up->unexpected_mcu=(mcu&BU_MCU_BUSY)!=0;
        if(up->unexpected_mcu || (mcu&BU_MCU_FLASH) ||
           !bu_read(d,0x40,4,&value) || (value&BU_GPIO_FLASH))return "flash-select";
        console_printf("rtl8822bu-power-host-download: mcu=%x gpio=%x\n",(uint64_t)mcu,(uint64_t)value);
    }
    if(!bu_update(d,3,1,0,4) || !bu_read(d,BU_MCU,4,&mcu))return "cpu-gate";
    up->unexpected_mcu=(mcu&BU_MCU_BUSY)!=0;
    if(mcu&(BU_MCU_FLASH|BU_MCU_BUSY)) {
        console_printf("rtl8822bu-power-cpu-gate: mcu=%x\n",(uint64_t)mcu);
        return "cpu-owner";
    }
    return bu_radio_off(d)?NULL:"radio-gate";
}

static bool bu_configure_out(struct bu_upload *up) {
    struct xhci_device *d=up->device;
    struct xhci_bulk_endpoint *ep=&up->out;
    void *page;
    if(!alloc_frame_zero(&ep->ring_phys,&page))return false;
    ep->ring=page;ep->dci=BU_OUT_DCI;ep->cycle=true;ep->packet=512;
    write_link_trb(ep->ring,ep->ring_phys,true);
    uint64_t phys=pmm_alloc_frames_tagged(3,PMM_FRAME_DMA);
    if(!phys)return false;
    /* One Normal TRB may not span a 64 KiB boundary. Keep its two pages
     * inside the same window even if the first allocation ends at 64 KiB. */
    if((phys&0xffffu)+BU_BLOCK_BYTES+BU_DESC_BYTES+1>0x10000u)phys+=PMM_FRAME_SIZE;
    up->packet_phys=phys;up->packet=pmm_phys_to_virt(phys);
    if(!up->packet)return false;
    zero_bytes(d->input_context,PMM_FRAME_SIZE);
    context_write32(d->input_context,0,1,1u|(1u<<BU_OUT_DCI));
    uint32_t slot0=context_read32(d->output_context,0,0);
    context_write32(d->input_context,1,0,(slot0&~(31u<<27))|(BU_OUT_DCI<<27));
    for(unsigned i=1;i<3;i++)context_write32(d->input_context,1,i,context_read32(d->output_context,0,i));
    rtl_cu_bulk_context(d,ep,2,BU_BLOCK_BYTES+BU_DESC_BYTES);
    const struct xhci_trb command={.parameter_low=(uint32_t)d->input_context_phys,
        .parameter_high=d->input_context_phys>>32,
        .control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)d->slot_id<<24)};
    uint64_t slot=UINT64_MAX;
    up->endpoint_ready=submit_command_trb(&command,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,&slot) && slot==d->slot_id;
    if(slot!=UINT64_MAX && slot!=d->slot_id)xhci.command_failed=true;
    return up->endpoint_ready;
}

static bool bu_send(struct bu_upload *up,const uint8_t *bytes,unsigned count) {
    struct xhci_device *d=up->device;
    struct xhci_bulk_endpoint *ep=&up->out;
    if(d->bulk_pending || device_removed(d) || !count || count>BU_BLOCK_BYTES ||
       ep->enqueue>=XHCI_TRB_COUNT-2)return false;
    unsigned padded=count+((count+BU_DESC_BYTES)%512==0),length=padded+BU_DESC_BYTES;
    uint8_t *p=up->packet;
    zero_bytes(p,length);
    p[0]=(uint8_t)padded;p[1]=(uint8_t)(padded>>8);p[2]=BU_DESC_BYTES;p[3]=4;p[5]=0x10;
    uint16_t checksum=0;
    for(unsigned i=0;i<32;i+=2)checksum^=p[i]|(uint16_t)p[i+1]<<8;
    p[28]=(uint8_t)checksum;p[29]=(uint8_t)(checksum>>8);
    copy_bytes(p+BU_DESC_BYTES,bytes,count);
    const struct xhci_trb trb={.parameter_low=(uint32_t)up->packet_phys,.parameter_high=up->packet_phys>>32,
        .status=length,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    d->bulk_trb=ring_push(ep->ring,ep->ring_phys,&ep->enqueue,&ep->cycle,&trb);
    d->bulk_dci=ep->dci;d->bulk_done=false;d->bulk_pending=true;
    d->bulk_completion=0;d->bulk_remaining=0;
    db_write32(d->slot_id,ep->dci);
    uint64_t start=timer_monotonic_us();
    for(unsigned i=0;i<XHCI_TRANSFER_TIMEOUT && d->bulk_pending;i++) {
        struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
        if((i&255u)==0 && timer_monotonic_us()-start>=250000u)break;
        __asm__ volatile("pause");
    }
    bool ok=d->bulk_done && !d->bulk_pending && d->bulk_completion==1 && !d->bulk_remaining;
    if(!ok)console_printf("rtl8822bu-out: length=%u completion=%u residue=%u pending=%u trb=%x\n",
        (uint64_t)length,(uint64_t)d->bulk_completion,(uint64_t)d->bulk_remaining,
        (uint64_t)d->bulk_pending,d->bulk_trb);
    if(ok)up->packets++;
    return ok;
}

static bool bu_load_block(struct bu_upload *up,const uint8_t *bytes,unsigned count,uint32_t dest,bool continued) {
    struct xhci_device *d=up->device;
    uint32_t beacon,sw,observed;
    if(!bu_read(d,BU_BCN,1,&beacon) || !bu_write(d,BU_BCN_HEAD,2,0x8000) ||
       !bu_read(d,BU_CR+1,1,&sw) || !bu_write(d,BU_CR+1,1,sw|1) ||
       !bu_write(d,BU_BCN,1,(beacon&~8u)|0x10u))return false;
    if(!bu_send(up,bytes,count) ||
       !bu_poll(d,BU_BCN_HEAD,4,0x8000,0x8000,100,&observed) ||
       !bu_write(d,BU_BCN_HEAD,2,0x8000) || !bu_write(d,BU_BCN,1,beacon) ||
       !bu_write(d,BU_CR+1,1,sw))return false;
    return bu_poll(d,BU_DDMA,4,BU_DDMA_OWN,0,100,&observed) &&
           bu_write(d,BU_DDMA_SRC,4,0x18780030) && bu_write(d,BU_DDMA_DST,4,dest) &&
           bu_write(d,BU_DDMA,4,BU_DDMA_OWN|BU_DDMA_CHECK|(continued?BU_DDMA_MORE:0)|count) &&
           bu_poll(d,BU_DDMA,4,BU_DDMA_OWN,0,100,&observed);
}

static bool bu_load_section(struct bu_upload *up,const uint8_t *image,unsigned length,uint32_t destination,uint8_t status) {
    struct xhci_device *d=up->device;
    uint32_t ddma;
    if(!bu_poll(d,BU_DDMA,4,BU_DDMA_OWN,0,100,&ddma) || !bu_write(d,BU_DDMA,4,ddma|BU_DDMA_RESET))
        return false;
    for(unsigned offset=0;offset<length;) {
        unsigned count=length-offset>BU_BLOCK_BYTES?BU_BLOCK_BYTES:length-offset;
        if(!wifi_hub_check_port(d,up->hub) ||
           !bu_load_block(up,image+offset,count,destination+offset,offset!=0))return false;
        offset+=count;
        up->bytes+=count;
    }
    if(!bu_read(d,BU_DDMA,4,&ddma) || (ddma&(BU_DDMA_BAD|BU_DDMA_OWN)))return false;
    console_printf("rtl8822bu-section: destination=%x bytes=%u checksum=1\n",(uint64_t)destination,(uint64_t)length);
    return bu_update(d,BU_MCU,1,0,status);
}

static const char *bu_download(struct bu_upload *up,const uint8_t *image) {
    struct xhci_device *d=up->device;
    uint32_t coex,hpq,cr,pages,rqpn,beacon,mcu;
    const char *power=bu_power_on(up);
    if(power) {
        console_printf("rtl8822bu-power: stage=%s ok=0\n",power);
        return "digital-power";
    }
    console_write("rtl8822bu-firmware-progress: digital power ready; preparing firmware DMA\n");
    if(!bu_write(d,0x01a0,1,0xfd) ||
       !bu_poll(d,0x1700,4,1u<<29,1u<<29,100,&coex) ||
       !bu_write(d,0x1700,4,0x800f0038) || !bu_read(d,0x1708,4,&coex) ||
       !bu_update(d,3,1,4,0) || !bu_update(d,0x1d,1,1,0))return "mcu-stop";
    if(!bu_read(d,0x010d,1,&hpq) || !bu_read(d,BU_CR,1,&cr) || cr ||
       !bu_read(d,0x0230,2,&pages) || !bu_read(d,0x022c,4,&rqpn) || !bu_read(d,BU_BCN,1,&beacon) ||
       !bu_write(d,0x010d,1,0xc0) || !bu_write(d,BU_CR,1,5) || !bu_write(d,0x1330,4,0x80000000u) ||
       !bu_write(d,0x0230,2,0x0200) || !bu_write(d,0x022c,4,rqpn|0x80000000u) ||
       !bu_write(d,BU_BCN,1,(beacon&~8u)|0x10u))return "download-queue";
    if(!bu_update(d,0x1082,1,1,0) || !bu_update(d,9,1,0x40,0) ||
       !bu_update(d,0x1082,1,1,1) || !bu_update(d,9,1,0x40,0x40) ||
       !bu_read(d,BU_MCU,2,&mcu) || !bu_write(d,BU_MCU,2,(mcu&0x3800)|1))return "download-enable";
    if(!bu_load_section(up,image+BU_FW_HEADER,BU_DMEM_BYTES,0x00200000,0x60))return "dmem";
    if(!bu_load_section(up,image+BU_FW_HEADER+BU_DMEM_BYTES,BU_IMEM_BYTES,0,0x18))return "imem";
    if(!bu_write(d,0x010d,1,hpq) || !bu_write(d,BU_CR,1,cr) || !bu_write(d,0x1330,4,0x80000000u) ||
       !bu_write(d,0x0230,2,pages) || !bu_write(d,0x022c,4,rqpn|0x80000000u) || !bu_write(d,BU_BCN,1,beacon) ||
       !bu_write(d,0x0210,4,4) || !bu_read(d,BU_MCU,2,&mcu) || (mcu&0x58)!=0x58 ||
       !bu_write(d,BU_MCU,2,(mcu|0x4000u)&~1u) || !bu_update(d,0x1d,1,1,1) ||
       !bu_update(d,3,1,4,4) || !bu_poll(d,0x1700,4,1u<<29,1u<<29,100,&mcu) ||
       !bu_write(d,0x1704,4,coex) || !bu_write(d,0x1700,4,0xc00f0038))return "mcu-release";
    if(!bu_poll(d,BU_MCU,4,0xcfffu,BU_MCU_READY,400,&up->mcu) ||
       !bu_radio_off(d) || !wifi_hub_check_port(d,up->hub))return "mcu-ready";
    return NULL;
}

static bool bu_stop(struct bu_upload *up) {
    struct xhci_device *d=up->device;
    if(!up->endpoint_ready)return !xhci.command_failed;
    uint64_t slot=UINT64_MAX;
    uint64_t end=up->out.ring_phys+up->out.enqueue*sizeof(struct xhci_trb);
    uint64_t pending=d->bulk_pending?d->bulk_trb:end;
    if(!submit_command(XHCI_TRB_TYPE_STOP_ENDPOINT,((uint32_t)d->slot_id<<24)|(BU_OUT_DCI<<16),&slot) ||
       slot!=d->slot_id)return false;
    uint64_t cursor=context_read32(d->output_context,BU_OUT_DCI,2) |
                    (uint64_t)context_read32(d->output_context,BU_OUT_DCI,3)<<32;
    if((context_read32(d->output_context,BU_OUT_DCI,0)&7)!=3 ||
       (cursor!=(pending|1) && cursor!=(end|1)))return false;
    d->bulk_pending=false;
    return true;
}

static bool bu_park(struct bu_upload *up) {
    struct xhci_device *d=up->device;
    uint32_t value;
    if(!up->power_touched)return true;
    return !up->unexpected_mcu && wifi_hub_check_port(d,up->hub) &&
           bu_poll(d,BU_DDMA,4,BU_DDMA_OWN,0,100,&value) &&
           bu_write(d,BU_CR,1,0) && bu_update(d,BU_CR+1,1,1,0) &&
           bu_update(d,3,1,4,0) && bu_update(d,0x1d,1,1,0) &&
           bu_update(d,BU_MCU,2,0x4001,0) && bu_read(d,BU_MCU,2,&value) && !(value&0x4001) &&
           bu_read(d,3,1,&value) && !(value&4) && bu_read(d,0x1d,1,&value) && !(value&1) &&
           bu_read(d,BU_CR,1,&value) && !value && bu_radio_off(d);
}

enum {
    BU_EFUSE_CTRL=0x30, BU_EFUSE_LDO=0x34, BU_EFUSE_READY=1u<<31,
    BU_EFUSE_BANK=3u<<8, BU_EFUSE_PROGRAM_POWER=1u<<31,
    BU_EFUSE_ADDR=0x3ff00, BU_EFUSE_DATA=0xff, BU_EFUSE_READ_MODE=0x31600000,
    BU_EFUSE_BYTES=1024-96, BU_EFUSE_LOGICAL_BYTES=768,
    BU_FEATURE_MAILBOX=0x01a0, BU_FEATURE_REPLY=0x19, BU_FEATURE_BYTES=13,
};

static bool bu_decode_efuse(const uint8_t *physical,unsigned length,uint8_t *logical) {
    for(unsigned i=0;i<BU_EFUSE_LOGICAL_BYTES;i++)logical[i]=0xff;
    for(unsigned offset=0;offset<length;) {
        unsigned header=physical[offset++];
        if(header==0xff)return true;
        unsigned block=header>>4,skip=header&15;
        if((header&31)==15) {
            if(offset==length)return false;
            unsigned extension=physical[offset++];
            if(extension==0xff)return true;
            block=((extension&0xf0)>>1)|(header>>5);
            skip=extension&15;
        }
        for(unsigned word=0;word<4;word++) {
            if(skip&(1u<<word))continue;
            unsigned index=block*8+word*2;
            if(length-offset<2 || index+1>=BU_EFUSE_LOGICAL_BYTES)return false;
            logical[index]=physical[offset++];
            logical[index+1]=physical[offset++];
        }
    }
    return false;
}

static const char *bu_capture_board(struct bu_upload *up) {
    struct xhci_device *d=up->device;
    uint8_t physical[BU_EFUSE_BYTES],logical[BU_EFUSE_LOGICAL_BYTES],feature[BU_FEATURE_BYTES];
    uint32_t ldo,control,cr;
    if(!bu_read(d,BU_CR,1,&cr) || cr || !bu_radio_off(d))return "radio-gate";
    if(!bu_read(d,BU_EFUSE_LDO,4,&ldo) || !bu_read(d,BU_EFUSE_CTRL,4,&control))return "efuse-entry";
    console_printf("rtl8822bu-board-entry: ldo=%x efuse=%x\n",(uint64_t)ldo,(uint64_t)control);
    /* Linux's trace has Wi-Fi bank selected and the programming supply off.
     * Admit that state and issue only read strobes in the observed mode. */
    const uint32_t mode_mask=~(BU_EFUSE_READY|BU_EFUSE_ADDR|BU_EFUSE_DATA);
    if((ldo&(BU_EFUSE_BANK|BU_EFUSE_PROGRAM_POWER)) || (control&mode_mask)!=BU_EFUSE_READ_MODE)
        return "efuse-mode";
    uint64_t start=timer_monotonic_us();
    for(unsigned i=0;i<BU_EFUSE_BYTES;i++) {
        if(timer_monotonic_us()-start>=2000000u)return "efuse-deadline";
        if(i%64==0 && !wifi_hub_check_port(d,up->hub))return "efuse-route";
        uint32_t address=i<<8;
        if(!bu_write(d,BU_EFUSE_CTRL,4,BU_EFUSE_READ_MODE|address) ||
           !bu_poll(d,BU_EFUSE_CTRL,4,BU_EFUSE_READY|BU_EFUSE_ADDR,BU_EFUSE_READY|address,40,&control)) {
            console_printf("rtl8822bu-board-read: address=%u ok=0\n",(uint64_t)i);
            return "efuse-read";
        }
        physical[i]=(uint8_t)control;
    }
    if(!bu_read(d,BU_EFUSE_LDO,4,&control) || control!=ldo)return "efuse-exit";
    if(!bu_decode_efuse(physical,sizeof physical,logical))return "efuse-map";
    unsigned id=logical[0]|(unsigned)logical[1]<<8;
    unsigned rfe=logical[0xca],xtal=logical[0xb9],plan=logical[0xb8];
    console_printf("rtl8822bu-board-efuse: bytes=%u id=%x rfe=%u xtal=%x plan=%x\n",
        (uint64_t)sizeof physical,(uint64_t)id,(uint64_t)rfe,(uint64_t)xtal,(uint64_t)plan);
    if(id!=0x8129 || rfe!=3 || xtal!=0x28 || plan!=0x34)return "board-profile";

    if(!bu_poll(d,BU_FEATURE_MAILBOX,1,0xff,BU_FEATURE_REPLY,400,&control))return "feature-id";
    for(unsigned i=0;i<sizeof feature;i++) {
        if(!bu_read(d,BU_FEATURE_MAILBOX+2+i,1,&control))return "feature-body";
        feature[i]=(uint8_t)control;
    }
    if(!bu_read(d,BU_FEATURE_MAILBOX,1,&control) || control!=BU_FEATURE_REPLY ||
       !bu_write(d,BU_FEATURE_MAILBOX,1,0) || !bu_read(d,BU_FEATURE_MAILBOX,1,&control) || control)
        return "feature-ack";
    unsigned hci=feature[4]&15,bw=feature[6]&7,nss=(feature[6]>>3)&3;
    unsigned antennas=feature[6]>>5,protocol=(feature[7]>>2)&3;
    console_printf("rtl8822bu-board-feature: hci=%u bw=%u nss=%u antennas=%u protocol=%u\n",
        (uint64_t)hci,(uint64_t)bw,(uint64_t)nss,(uint64_t)antennas,(uint64_t)protocol);
    if(hci!=2 || bw!=7 || nss!=2 || antennas!=2 || protocol!=3)return "feature-profile";
    return wifi_hub_check_port(d,up->hub) && bu_radio_off(d)?NULL:"radio-exit";
}

#include "xhci_rtl8822bu_phy.inc"
#include "xhci_rtl8822bu_rx.inc"

enum bu_mode { BU_FIRMWARE_ONLY, BU_BOARD_ONLY, BU_PHY_INIT, BU_PASSIVE };

static bool bu_capture(const uint8_t *image,uint64_t size,enum bu_mode mode,
                       const uint8_t *phy,uint64_t phy_size,bool *return_safe) {
    struct bu_upload up={0};
    const char *failure="identity";
    const char *board_failure="not-run";
    const char *phy_failure="not-run";
    const char *rx_failure="not-run";
    unsigned phy_written=0;
    bool radio_parked=true,rx_retired=true;
    uint64_t flags=event_irq_save();
    for(unsigned i=0;i<XHCI_MAX_DEVICES;i++) {
        if(xhci.devices[i].rtl8822bu_identified){up.device=&xhci.devices[i];break;}
    }
    *return_safe=true;
    struct xhci_device *d=up.device;
    if(d) {
        up.hub=device_by_slot(d->parent_slot_id);
        failure="admission";
        if(up.hub && !d->rtl8822bu_firmware_attempted && !d->control_failed &&
           !d->control_pending && !d->ep0_quarantined && !d->configured &&
           !xhci.sync_users && !xhci.command_pending && !strict_control.device &&
           timer_reference_ready() && wifi_hub_path_present(d)) {
            d->rtl8822bu_firmware_attempted=true;
            failure="image";
            if(bu_image_valid(image,size)) {
                d->rtl8822bu_firmware_active=true;
                failure="endpoint";
                if(wifi_hub_check_port(d,up.hub) && bu_configure_out(&up))failure=bu_download(&up,image);
                if(mode!=BU_FIRMWARE_ONLY && !failure) {
                    console_write("rtl8822bu-board-progress: reading eFuse and firmware features\n");
                    board_failure=bu_capture_board(&up);
                }
                if(mode>=BU_PHY_INIT && !board_failure) {
                    phy_failure="profile";
                    if(bu_phy_profile_valid(phy,phy_size)) {
                        phy_failure="tx-pause";
                        if(bu_write(d,BU_TXPAUSE,1,0xff)) {
                            console_write("rtl8822bu-phy-progress: loading RFE type 3 tables with MAC traffic disabled\n");
                            bool phy_touched=false;
                            phy_failure=bu_phy_run(&up,phy,&phy_written,&phy_touched);
                            if(mode==BU_PASSIVE && !phy_failure)rx_failure=bu_receive(&up,&rx_retired);
                            if(phy_touched)radio_parked=bu_phy_park(&up);
                        }
                    }
                }
                up.stopped=bu_stop(&up);
                up.parked=up.stopped && bu_park(&up);
                *return_safe=radio_parked && rx_retired && up.stopped && !d->ep0_quarantined && !xhci.command_failed &&
                    (!up.power_touched || up.parked);
                if(!failure && !*return_safe)failure="retirement";
                d->rtl8822bu_firmware_active=false;
            }
        }
    }
    event_irq_restore(flags);
    console_printf("rtl8822bu-firmware: result=%s bytes=%u packets=%u mcu=%x stopped=%u parked=%u safe=%u\n",
        failure?failure:"ready",(uint64_t)up.bytes,(uint64_t)up.packets,(uint64_t)up.mcu,
        (uint64_t)up.stopped,(uint64_t)up.parked,(uint64_t)*return_safe);
    if(mode!=BU_FIRMWARE_ONLY)console_printf("rtl8822bu-board: result=%s\n",board_failure?board_failure:"ready");
    if(mode>=BU_PHY_INIT)console_printf("rtl8822bu-phy: result=%s writes=%u radio_parked=%u\n",
        phy_failure?phy_failure:"ready",(uint64_t)phy_written,(uint64_t)radio_parked);
    if(mode==BU_PASSIVE)console_printf("rtl8822bu-passive: result=%s retired=%u\n",
        rx_failure?rx_failure:"ready",(uint64_t)rx_retired);
    return !failure && (mode==BU_FIRMWARE_ONLY || !board_failure) &&
           (mode<BU_PHY_INIT || !phy_failure) && (mode!=BU_PASSIVE || !rx_failure);
}

bool xhci_rtl8822bu_capture_firmware(const uint8_t *image,uint64_t size,bool *return_safe) {
    return bu_capture(image,size,BU_FIRMWARE_ONLY,NULL,0,return_safe);
}

bool xhci_rtl8822bu_capture_board(const uint8_t *image,uint64_t size,bool *return_safe) {
    return bu_capture(image,size,BU_BOARD_ONLY,NULL,0,return_safe);
}

bool xhci_rtl8822bu_capture_phy(const uint8_t *image,uint64_t size,const uint8_t *phy,uint64_t phy_size,bool *return_safe) {
    return bu_capture(image,size,BU_PHY_INIT,phy,phy_size,return_safe);
}

bool xhci_rtl8822bu_capture_passive(const uint8_t *image,uint64_t size,const uint8_t *phy,uint64_t phy_size,bool *return_safe) {
    return bu_capture(image,size,BU_PASSIVE,phy,phy_size,return_safe);
}
