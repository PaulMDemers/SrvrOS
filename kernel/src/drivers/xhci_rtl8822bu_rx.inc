/* Receive-only validation for the measured A6150. The trial tunes both
 * synthesizers on 161 and 10, then samples channel 10 through bulk IN. */
enum {
    BU_RX_DCI=9, BU_RX_BYTES=32768, BU_RX_TDS=32, BU_RX_CR=0x8a,
    BU_RF_MASK=0xfffff, BU_RF18_TUNE=0x70fff,
};

struct bu_rx {
    struct xhci_device *device;
    struct xhci_bulk_endpoint ep;
    uint8_t *buffer;
    uint64_t phys,trb;
    unsigned posted,completed,actual,records,beacons,target,bad;
    bool configured,pending,failed,stopping;
};
static struct bu_rx *bu_rx_owner;

static bool bu_rf_write(struct xhci_device *d,unsigned path,unsigned reg,uint32_t value) {
    return bu_write(d,path?0xe90:0xc90,4,(reg<<20)|(value&BU_RF_MASK)) && timer_delay_us(13);
}

static bool bu_rf_update(struct xhci_device *d,unsigned reg,uint32_t mask,uint32_t value) {
    uint32_t old;
    return bu_read(d,0x2800+reg*4,4,&old) && (old&BU_RF_MASK)!=BU_RF_MASK &&
           bu_rf_write(d,0,reg,(old&~mask)|value);
}

static bool bu_tune(struct bu_upload *up,unsigned channel) {
    struct xhci_device *d=up->device;
    bool high=channel==161;
    if((!high && channel!=10) || !bu_phy_tx_inhibited(d) || !bu_phy_powered(d) ||
       !wifi_hub_check_port(d,up->hub))return false;
    uint32_t old;
    if(!bu_read(d,0x2860,4,&old) || (old&BU_RF_MASK)==BU_RF_MASK)return false;
    uint32_t channel_bits=(high?0x50d00u:0xc00u)|channel;
    uint32_t tuned=(old&~BU_RF18_TUNE)|channel_bits;
    if(!bu_rf_update(d,0xbe,0x38000,high?0x28000:0) || !bu_rf_update(d,0xdf,1u<<18,0) ||
       !bu_rf_write(d,0,0x18,tuned) || !bu_rf_write(d,1,0x18,tuned) ||
       !bu_rf_update(d,0xb8,1u<<19,0) || !bu_rf_update(d,0xb8,1u<<19,1u<<19))return false;
    uint32_t a,b;
    if(!bu_read(d,0x2860,4,&a) || !bu_read(d,0x2c60,4,&b))return false;
    bool ok=(a&BU_RF_MASK)==(tuned&BU_RF_MASK) && (b&BU_RF_MASK)==(tuned&BU_RF_MASK);
    console_printf("rtl8822bu-tune: channel=%u rf18_a=%x rf18_b=%x matched=%u\n",
        (uint64_t)channel,(uint64_t)a,(uint64_t)b,(uint64_t)ok);
    return ok && bu_phy_tx_inhibited(d);
}

/* 20 MHz, internal front-end, two receive paths. MAC TX/DMA stays disabled. */
static bool bu_rx_baseband(struct xhci_device *d) {
    if(!bu_update(d,0xa80,4,1u<<18,0) || !bu_update(d,0x454,1,0x80,0) ||
       !bu_update(d,0x814,4,0xfc00,15u<<10) || !bu_update(d,0x958,4,31,0) ||
       !bu_update(d,0x860,4,0x1ffe0000,0x96au<<17) || !bu_write(d,0xa24,4,0x384f6577) ||
       !bu_update(d,0xa28,4,0xffff,0x1525) || !bu_update(d,0xcbc,4,0x300,0x200) ||
       !bu_update(d,0x8ac,4,0x003003ff,0) || !bu_update(d,0x8c4,4,1u<<30,1u<<30) ||
       !bu_write(d,0x483,1,0) || !bu_update(d,0x668,4,0x180,0) ||
       !bu_update(d,0x24,4,3u<<20,0) || !bu_write(d,0x55c,1,80) || !bu_write(d,0x638,1,80) ||
       !bu_update(d,0x948,4,3u<<28,2u<<28) || !bu_update(d,0x94c,4,3u<<28,2u<<28))return false;
    for(unsigned path=0;path<=0x200;path+=0x200) {
        uint32_t gain;
        if(!bu_read(d,0xc50+path,4,&gain) || (gain&127)<2 ||
           !bu_write(d,0xc50+path,4,gain-2) || !bu_write(d,0xc50+path,4,gain) ||
           !bu_update(d,0xc20+path,4,1u<<31,1u<<31) || !bu_update(d,0xcb0+path,4,0xffffff,0x745774) ||
           !bu_update(d,0xcb4+path,4,0xff00,0x5700) || !bu_update(d,0xcbc+path,4,0xc3f,0) ||
           !bu_update(d,0xca0+path,4,0xffff,0xa501))return false;
    }
    return bu_update(d,BU_RXPSEL,4,0xff,0) && bu_update(d,BU_RXPSEL,4,0xff,0x33) &&
           bu_write(d,0x82c,4,0x75c97010) && bu_write(d,0x830,4,0x79a0eaac) &&
           bu_write(d,0x838,4,0x87746341);
}

static bool bu_rx_configure(struct bu_rx *rx) {
    struct xhci_device *d=rx->device;
    struct xhci_bulk_endpoint *ep=&rx->ep;
    void *page;
    if(!alloc_frame_zero(&ep->ring_phys,&page))return false;
    ep->ring=page;ep->dci=BU_RX_DCI;ep->cycle=true;ep->packet=512;
    write_link_trb(ep->ring,ep->ring_phys,true);
    const unsigned pages=BU_RX_BYTES/PMM_FRAME_SIZE;
    uint64_t phys=pmm_alloc_frames_tagged(2*pages-1,PMM_FRAME_DMA);
    if(!phys)return false;
    if((phys&0xffffu)+BU_RX_BYTES>0x10000u)phys=(phys+0xffffu)&~0xffffull;
    rx->phys=phys;rx->buffer=pmm_phys_to_virt(phys);
    if(!rx->buffer)return false;
    zero_bytes(d->input_context,PMM_FRAME_SIZE);
    context_write32(d->input_context,0,1,1u|(1u<<BU_RX_DCI));
    for(unsigned i=0;i<3;i++)context_write32(d->input_context,1,i,context_read32(d->output_context,0,i));
    rtl_cu_bulk_context(d,ep,6,BU_RX_BYTES);
    const struct xhci_trb command={.parameter_low=(uint32_t)d->input_context_phys,
        .parameter_high=d->input_context_phys>>32,
        .control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)d->slot_id<<24)};
    uint64_t slot=UINT64_MAX;
    rx->configured=submit_command_trb(&command,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,&slot) && slot==d->slot_id;
    if(slot!=UINT64_MAX && slot!=d->slot_id)xhci.command_failed=true;
    return rx->configured;
}

static bool bu_rx_event(struct xhci_device *d,uint8_t dci,uint64_t pointer,uint8_t cc,uint32_t residue,bool ed) {
    struct bu_rx *rx=bu_rx_owner;
    if(!rx || d!=rx->device || dci!=BU_RX_DCI)return false;
    if(rx->stopping && rx->pending && pointer==rx->trb && !ed && (cc==26 || cc==27 || cc==28))return true;
    if(!rx->pending || pointer!=rx->trb || ed || (cc!=1 && cc!=13) || residue>BU_RX_BYTES ||
       (cc==1 && residue)) {
        rx->failed=true;
        return true;
    }
    rx->actual=BU_RX_BYTES-residue;
    rx->completed++;
    rx->pending=false;
    return true;
}

static void bu_rx_post(struct bu_rx *rx) {
    struct xhci_bulk_endpoint *ep=&rx->ep;
    const struct xhci_trb trb={.parameter_low=(uint32_t)rx->phys,.parameter_high=rx->phys>>32,
        .status=BU_RX_BYTES,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    struct xhci_trb *entry=&ep->ring[ep->enqueue];
    rx->trb=ring_push_owned(ep->ring,ep->ring_phys,&ep->enqueue,&ep->cycle,&trb,true);
    rx->pending=true;rx->posted++;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    ((volatile struct xhci_trb *)entry)->control=(trb.control&~1u)|ep->cycle;
    db_write32(rx->device->slot_id,BU_RX_DCI);
}

static void bu_rx_parse(struct bu_rx *rx) {
    const uint8_t *data=rx->buffer;
    size_t length=rx->actual;
    while(length>=24) {
        uint32_t desc=rtl_cu_le32(data),meta=rtl_cu_le32(data+8);
        unsigned prefix=24+8*((desc>>16)&15)+((desc>>24)&3),mpdu=desc&0x3fff;
        if(!mpdu || prefix+mpdu>length){rx->bad++;return;}
        rx->records++;
        const uint8_t *frame=data+prefix;
        if(!(meta&(1u<<28)) && !(desc&0xc000) && mpdu>=40 && (frame[0]&0xfc)==0x80) {
            bool target=false,on_channel=false,valid=true;
            for(unsigned at=36,end=mpdu-4;at<end;) {
                if(end-at<2 || frame[at+1]>end-at-2){valid=false;break;}
                unsigned tag=frame[at],len=frame[at+1];
                if(tag==0 && len==9 && bytes_equal(frame+at+2,(const uint8_t *)"Demers2.4",9))target=true;
                if((tag==3 && len==1) || (tag==61 && len>=1))on_channel|=frame[at+2]==10;
                at+=2+len;
            }
            if(valid){rx->beacons++;rx->target+=target && on_channel;}
        }
        size_t used=(prefix+mpdu+7u)&~7u;
        if(used>=length)return;
        data+=used;length-=used;
    }
}

static bool bu_rx_stop(struct bu_rx *rx) {
    struct xhci_device *d=rx->device;
    if(!rx->configured)return !xhci.command_failed;
    uint64_t slot=UINT64_MAX;
    rx->stopping=true;
    bool ok=submit_command(XHCI_TRB_TYPE_STOP_ENDPOINT,((uint32_t)d->slot_id<<24)|(BU_RX_DCI<<16),&slot);
    rx->stopping=false;
    if(!ok || slot!=d->slot_id)return false;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    uint64_t cursor=context_read32(d->output_context,BU_RX_DCI,2) |
                    (uint64_t)context_read32(d->output_context,BU_RX_DCI,3)<<32;
    uint64_t next=rx->ep.ring_phys+rx->ep.enqueue*sizeof(struct xhci_trb);
    return (context_read32(d->output_context,BU_RX_DCI,0)&7)==3 &&
           (cursor==(next|1u) || (rx->pending && cursor==(rx->trb|1u)));
}

static const char *bu_receive(struct bu_upload *up,bool *retired) {
    struct xhci_device *d=up->device;
    struct bu_rx rx={.device=d};
    if(!bu_tune(up,161) || !bu_rx_baseband(d) || !bu_tune(up,10))return "channel";
    if(!bu_phy_tx_inhibited(d) || bu_rx_owner || !bu_rx_configure(&rx))return "endpoint";
    bu_rx_owner=&rx;
    /* High-speed DMA, no aggregation, and accept only broadcast beacons. */
    uint32_t cr;
    const char *failure="rx-setup";
    bool enabled=bu_write(d,0x290,1,0x1e) && bu_update(d,0x10c,1,4,0) &&
        bu_write(d,0x11c,4,0x5eff) && bu_update(d,0x115,1,15,15) &&
        bu_write(d,0x60c,1,0x18) && bu_write(d,0x60f,1,4) &&
        bu_write(d,0x608,4,0x90000008u) && bu_write(d,0x6a0,4,1u<<8) && bu_write(d,0x6a4,2,0) &&
        bu_phy_tx_inhibited(d) && bu_write(d,BU_CR,1,BU_RX_CR) && bu_read(d,BU_CR,1,&cr) && cr==BU_RX_CR;
    if(enabled) {
        console_write("rtl8822bu-rx-progress: sampling channel 10 with MAC transmit disabled\n");
        uint64_t start=timer_monotonic_us();
        bu_rx_post(&rx);
        for(unsigned spin=0;spin<XHCI_TRANSFER_TIMEOUT && !rx.failed;spin++) {
            struct xhci_trb event;
            if(poll_event(&event))dispatch_transfer_event(&event);
            if(rx.failed)break;
            if(!rx.pending) {
                __atomic_thread_fence(__ATOMIC_ACQUIRE);
                bu_rx_parse(&rx);
                if(rx.target || rx.posted==BU_RX_TDS)break;
                bu_rx_post(&rx);
            }
            if((spin&255u)==0 && (timer_monotonic_us()-start>=600000u || device_removed(d)))break;
            __asm__ volatile("pause");
        }
        failure=rx.failed?"rx-event":rx.target?NULL:"no-target";
    }
    bool gated=bu_write(d,BU_CR,1,0) && bu_phy_tx_inhibited(d);
    bool stopped=bu_rx_stop(&rx);
    bu_rx_owner=NULL;
    *retired=gated && stopped;
    console_printf("rtl8822bu-rx: result=%s posted=%u completed=%u records=%u beacons=%u target=%u bad=%u gated=%u stopped=%u\n",
        failure?failure:"ready",(uint64_t)rx.posted,(uint64_t)rx.completed,(uint64_t)rx.records,
        (uint64_t)rx.beacons,(uint64_t)rx.target,(uint64_t)rx.bad,(uint64_t)gated,(uint64_t)stopped);
    return *retired?failure:"rx-retirement";
}
