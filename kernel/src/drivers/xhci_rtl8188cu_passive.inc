/* Observed single-chain RTL8188CUS setup and a fixed US channel 10 test. */
#define RTL_CU_PROFILE_WRITES 495u
#define RTL_CU_PROFILE_BYTES (RTL_CU_PROFILE_WRITES*6u)
#define RTL_CU_RX_DCI 3u
#define RTL_CU_RX_BYTES 4096u
/* Enable the MAC protocol block during RX with all TX gates closed. */
#define RTL_CU_PROTOCOL_ENABLE (1u<<4)
#define RTL_CU_SCHEDULE_ENABLE (1u<<5)
#define RTL_CU_RX_ENABLE (0x8au|RTL_CU_PROTOCOL_ENABLE)
#define RTL_CU_TX_GATES (RTL8188CU_TX_ENABLE|RTL_CU_SCHEDULE_ENABLE)
#define RTL_CU_TX_PAUSE 0x0522u
#define RTL_CU_CHANNEL 10u
#define RTL_CU_LLT 0x01e0u
#define RTL_CU_LLT_WRITE (1u<<30)
#define RTL_CU_LLT_BUSY (3u<<30)
#define RTL_CU_PACKET_BOUNDARY 0xf9u
#define RTL_CU_MGMT_RCR 0x200eu
#define RTL_CU_MGMT_MAP 0xffffu
#define RTL_CU_RF_DATA_MASK 0xfffffu
#define RTL_CU_RF_START_CHANNEL 0x7401u
#define RTL_CU_LC_START 0x8000u

enum rtl_cu_sample_phase {
    RTL_CU_ARMED,RTL_CU_PENDING,RTL_CU_BEFORE_STOP,RTL_CU_AFTER_STOP,RTL_CU_SAMPLE_COUNT
};
enum rtl_cu_rx_register { RTL_CU_CR,RTL_CU_RCR,RTL_CU_RXFF_PTR,RTL_CU_RXPKT_NUM,RTL_CU_RXDMA_STATUS,RTL_CU_RX_REG_COUNT };
enum rtl_cu_rx_admission {
    RTL_CU_FILTER_RCR,RTL_CU_FILTER_MGT,RTL_CU_FILTER_CTL,RTL_CU_FILTER_DATA,RTL_CU_DRIVER_INFO,
    RTL_CU_FPGA0_RFMOD,RTL_CU_FPGA1_RFMOD,RTL_CU_OFDM_TRX,RTL_CU_ADMISSION_COUNT
};
struct rtl_cu_rx_sample {
    uint64_t start_us,end_us,dequeue,trb;
    uint32_t regs[RTL_CU_RX_REG_COUNT];
    unsigned state,valid,posted,completed_before,completed_after,events;
    bool present,pending;
};

static struct {
    struct xhci_device *owner;
    struct xhci_bulk_endpoint ep;
    uint8_t *buffer;
    uint64_t physical,trb,started_us,first_completion_us;
    unsigned posted,completed,actual,beacons,matches,rejected;
    struct rtl_cu_rx_sample samples[RTL_CU_SAMPLE_COUNT];
    uint64_t event_trb;
    unsigned events,event_code,event_residue;
    uint32_t rx_cr;
    uint32_t startup_rf_channel;
    uint64_t profile_wait_us;
    unsigned profile_waits;
    bool startup_rf_valid,profile_started;
    uint64_t post_lc_wait_us;
    uint32_t post_lc_channel,gain[2];
    unsigned gain_valid;
    bool post_lc_attempted,post_lc_triggered,post_lc_read_valid;
    uint32_t admission[RTL_CU_ADMISSION_COUNT];
    unsigned admission_valid;
    bool admission_attempted,admission_ok;
    bool event_data,rx_cr_valid;
    bool configured,pending,failed;
} rtl_passive;

static bool rtl_cu_verify_receive_setup(struct xhci_device *d) {
    /* Match the measured management filter and the pinned baseband layout.
     * Leave control/data maps and optional descriptor trailers disabled. */
    static const struct { uint16_t address,width; uint32_t expected; } regs[RTL_CU_ADMISSION_COUNT]={
        [RTL_CU_FILTER_RCR]={0x608,4,RTL_CU_MGMT_RCR}, [RTL_CU_FILTER_MGT]={0x6a0,2,RTL_CU_MGMT_MAP},
        [RTL_CU_FILTER_CTL]={0x6a2,2,0}, [RTL_CU_FILTER_DATA]={0x6a4,2,0}, [RTL_CU_DRIVER_INFO]={0x60f,1,0},
        [RTL_CU_FPGA0_RFMOD]={0x800,4,0x83040000}, [RTL_CU_FPGA1_RFMOD]={0x900,4,0},
        [RTL_CU_OFDM_TRX]={0xc04,4,0x03a05611},
    };
    rtl_passive.admission_attempted=true;
    bool match=true;
    for(unsigned i=0;i<RTL_CU_ADMISSION_COUNT;i++) {
        if(!rtl8188cu_read(d,regs[i].address,regs[i].width,&rtl_passive.admission[i]))return false;
        rtl_passive.admission_valid|=1u<<i;
        if(rtl_passive.admission[i]!=regs[i].expected)match=false;
    }
    rtl_passive.admission_ok=match;
    return match;
}

static uint64_t rtl_cu_rx_dequeue(const struct xhci_device *d) {
    return context_read32(d->output_context,RTL_CU_RX_DCI,2) |
        (uint64_t)context_read32(d->output_context,RTL_CU_RX_DCI,3)<<32;
}

static void rtl_cu_rx_snapshot(struct xhci_device *d,enum rtl_cu_sample_phase phase,bool read_chip) {
    /* These USB vendor requests read status only. The endpoint context comes
     * first, and completions may advance while EP0 reads are in flight. */
    static const struct { uint16_t address,width; } regs[RTL_CU_RX_REG_COUNT]={
        [RTL_CU_CR]={RTL8188CU_MAC_CONTROL,2}, [RTL_CU_RCR]={0x608,4},
        [RTL_CU_RXFF_PTR]={0x11c,4}, [RTL_CU_RXPKT_NUM]={0x284,4}, [RTL_CU_RXDMA_STATUS]={0x288,4},
    };
    struct rtl_cu_rx_sample *sample=&rtl_passive.samples[phase];
    sample->present=true;
    sample->start_us=timer_monotonic_us();
    sample->state=context_read32(d->output_context,RTL_CU_RX_DCI,0)&7;
    sample->dequeue=rtl_cu_rx_dequeue(d);
    sample->trb=rtl_passive.trb;
    sample->pending=rtl_passive.pending;
    sample->posted=rtl_passive.posted;
    sample->completed_before=rtl_passive.completed;
    sample->events=rtl_passive.events;
    if(read_chip && !device_removed(d) && !rtl_passive.failed && !d->control_failed && !d->ep0_quarantined) {
        for(unsigned i=0;i<RTL_CU_RX_REG_COUNT;i++) {
            if(!rtl8188cu_read(d,regs[i].address,regs[i].width,&sample->regs[i]))break;
            sample->valid|=1u<<i;
        }
    }
    sample->completed_after=rtl_passive.completed;
    sample->end_us=timer_monotonic_us();
}

static void rtl_cu_rx_log(void) {
    static const char *const phases[]={"armed","pending","before-stop","after-stop"};
    if(rtl_passive.profile_started)
        console_printf("rtl8188cu-rf-settle: waits=%u wait_us=%u read_valid=%u rf_channel=%x\n",
            (uint64_t)rtl_passive.profile_waits,rtl_passive.profile_wait_us,
            (uint64_t)rtl_passive.startup_rf_valid,(uint64_t)rtl_passive.startup_rf_channel);
    if(rtl_passive.post_lc_attempted)
        console_printf("rtl8188cu-post-lc: triggered=%u wait_us=%u read_valid=%u rf_channel=%x gain_valid=%u gain0=%x gain1=%x\n",
            (uint64_t)rtl_passive.post_lc_triggered,rtl_passive.post_lc_wait_us,
            (uint64_t)rtl_passive.post_lc_read_valid,(uint64_t)rtl_passive.post_lc_channel,
            (uint64_t)rtl_passive.gain_valid,(uint64_t)rtl_passive.gain[0],(uint64_t)rtl_passive.gain[1]);
    const uint32_t *r=rtl_passive.admission;
    if(rtl_passive.admission_attempted)
        console_printf("rtl8188cu-rx-setup: ready=%u valid=%x rcr=%x mgt=%x ctl=%x data=%x drvinfo=%x fpga0=%x fpga1=%x ofdm_trx=%x\n",
            (uint64_t)rtl_passive.admission_ok,(uint64_t)rtl_passive.admission_valid,
            (uint64_t)r[RTL_CU_FILTER_RCR],(uint64_t)r[RTL_CU_FILTER_MGT],(uint64_t)r[RTL_CU_FILTER_CTL],
            (uint64_t)r[RTL_CU_FILTER_DATA],(uint64_t)r[RTL_CU_DRIVER_INFO],
            (uint64_t)r[RTL_CU_FPGA0_RFMOD],(uint64_t)r[RTL_CU_FPGA1_RFMOD],(uint64_t)r[RTL_CU_OFDM_TRX]);
    for(unsigned i=0;i<RTL_CU_SAMPLE_COUNT;i++) {
        const struct rtl_cu_rx_sample *s=&rtl_passive.samples[i];
        if(!s->present)continue;
        console_printf("rtl8188cu-rx-context: phase=%s start_us=%u end_us=%u state=%u ring=%x dequeue=%x trb=%x pending=%u posted=%u completed_before=%u completed_after=%u events=%u\n",
            phases[i],s->start_us,s->end_us,(uint64_t)s->state,rtl_passive.ep.ring_phys,s->dequeue,s->trb,
            (uint64_t)s->pending,(uint64_t)s->posted,(uint64_t)s->completed_before,
            (uint64_t)s->completed_after,(uint64_t)s->events);
        if(i!=RTL_CU_AFTER_STOP)
            console_printf("rtl8188cu-rx-chip: phase=%s valid=%x cr=%x rcr=%x rxff_ptr=%x rxpkt_num=%x rxdma_status=%x\n",
                phases[i],(uint64_t)s->valid,(uint64_t)s->regs[RTL_CU_CR],(uint64_t)s->regs[RTL_CU_RCR],
                (uint64_t)s->regs[RTL_CU_RXFF_PTR],(uint64_t)s->regs[RTL_CU_RXPKT_NUM],(uint64_t)s->regs[RTL_CU_RXDMA_STATUS]);
    }
    if(rtl_passive.configured) {
        console_printf("rtl8188cu-rx-gate: valid=%u expected=%x observed=%x\n",
            (uint64_t)rtl_passive.rx_cr_valid,(uint64_t)RTL_CU_RX_ENABLE,(uint64_t)rtl_passive.rx_cr);
        console_printf("rtl8188cu-rx-event: count=%u last_trb=%x code=%u residue=%u event_data=%u\n",
            (uint64_t)rtl_passive.events,rtl_passive.event_trb,(uint64_t)rtl_passive.event_code,
            (uint64_t)rtl_passive.event_residue,(uint64_t)rtl_passive.event_data);
    }
}

static uint32_t rtl_cu_le32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}

static bool rtl_cu_profile_valid(const uint8_t *blob,uint64_t size) {
    static const uint8_t expected[]={
        0xb1,0xe8,0x61,0xa2,0x4a,0xb1,0x8f,0x03,0xa0,0xe6,0x43,0xc5,0x09,0x9b,0xd1,0x1b,
        0x75,0x6a,0xd1,0x5d,0xa0,0x23,0xca,0xf6,0x88,0xe1,0xa9,0xc7,0xb0,0x96,0xc3,0xcb,
    };
    if(!blob || size!=RTL_CU_PROFILE_BYTES)return false;
    uint8_t digest[sizeof expected];
    sha256_digest(blob,size,digest);
    for(unsigned i=0;i<sizeof digest;i++)if(digest[i]!=expected[i])return false;
    for(unsigned i=0;i<RTL_CU_PROFILE_WRITES;i++) {
        const uint8_t *row=blob+6*i;
        unsigned reg=row[0]|(unsigned)row[1]<<8;
        uint32_t value=rtl_cu_le32(row+2);
        if(reg<0x800 || reg>0xffc || (reg&3) ||
           (reg==0xe28 && value) || (reg==0xd00 && (value&0x70000000)))return false;
    }
    return true;
}

static bool rtl_cu_tx_parked(struct xhci_device *d) {
    uint32_t mac,pause;
    return rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&mac) && !(mac&RTL_CU_TX_GATES) &&
        rtl8188cu_read(d,RTL_CU_TX_PAUSE,1,&pause) && pause==0xff;
}

/* The CU packet buffer needs its page links even with MAC TX and TX DMA
 * closed. Use the layout captured on this exact board and set both bounds
 * while MAC RX is still off. */
static bool rtl_cu_packet_buffer(struct xhci_device *d,struct xhci_device *hub,
                                 unsigned *linked) {
    uint32_t mac,value;
    if(!rtl_cu_tx_parked(d) ||
       !rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&mac) || (mac&RTL_CU_RX_ENABLE))return false;
    const uint64_t start=timer_monotonic_us();
    for(unsigned page=0;page<256;page++) {
        unsigned next=page+1;
        if(page==RTL_CU_PACKET_BOUNDARY-1)next=0xff;
        if(page==0xff)next=RTL_CU_PACKET_BOUNDARY;
        if(timer_monotonic_us()-start>=1000000 ||
           ((page&63)==0 && !rtl8188cu_check_port(d,hub)) ||
           !rtl8188cu_write_value(d,RTL_CU_LLT,4,RTL_CU_LLT_WRITE|(page<<8)|next) ||
           !rtl8188cu_poll(d,RTL_CU_LLT,4,RTL_CU_LLT_BUSY,0,8,&value) ||
           ((value>>8)&0xff)!=page)return false;
        (*linked)++;
    }
    return rtl8188cu_write_value(d,0x114,1,RTL_CU_PACKET_BOUNDARY) &&
           rtl8188cu_write_value(d,0x116,2,0x27ff) &&
           rtl8188cu_write_value(d,0x104,1,0x11) &&
           rtl8188cu_read(d,0x114,1,&value) && value==RTL_CU_PACKET_BOUNDARY &&
           rtl8188cu_read(d,0x116,2,&value) && value==0x27ff &&
           rtl8188cu_read(d,0x104,1,&value) && value==0x11;
}

static bool rtl_cu_rf_read(struct xhci_device *d,unsigned reg,uint32_t *value) {
    uint32_t mode,config;
    const uint32_t edge=1u<<31;
    if(!rtl8188cu_read(d,0x824,4,&config) ||
       !rtl8188cu_write_value(d,0x824,4,config&~edge) || !timer_delay_us(1000) ||
       !rtl8188cu_write_value(d,0x824,4,(config&~0x7f800000u)|((uint32_t)reg<<23)|edge) ||
       !timer_delay_us(1000) || !rtl8188cu_write_value(d,0x824,4,config|edge) ||
       !timer_delay_us(1000) || !rtl8188cu_read(d,0x820,4,&mode))return false;
    return rtl8188cu_read(d,(mode&(1u<<8))?0x8b8:0x8a0,4,value);
}

static uint32_t rtl_cu_profile_wait(unsigned row) {
    /* Zero-based rows in the digest-pinned profile. Hold RF calibration and
     * the following RF write for 100 ms, then preserve the HSSI edge gaps. */
    if(row==481 || row==482)return 100000;
    if(row>=487 && row<=492)return 1000;
    return 0;
}

static bool rtl_cu_post_profile_lc(struct xhci_device *d,struct xhci_device *hub) {
    uint32_t test_mode,mac;
    rtl_passive.post_lc_attempted=true;
    /* Use the normal 1T LC branch observed on this board. No IQK test
     * transmission, RF mode override or TX queue release is needed. */
    if(!rtl_cu_tx_parked(d) ||
       !rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&mac) || (mac&RTL_CU_RX_ENABLE) ||
       !rtl8188cu_read(d,0xd03,1,&test_mode) || (test_mode&0x70) ||
       !rtl8188cu_write_value(d,0x840,4,0x01800000u|RTL_CU_RF_START_CHANNEL|RTL_CU_LC_START))return false;
    rtl_passive.post_lc_triggered=true;
    uint64_t before=timer_monotonic_us();
    if(!timer_delay_us(100000))return false;
    rtl_passive.post_lc_wait_us=timer_monotonic_us()-before;
    if(!rtl8188cu_check_port(d,hub) || !rtl_cu_tx_parked(d) ||
       !rtl_cu_rf_read(d,0x18,&rtl_passive.post_lc_channel))return false;
    rtl_passive.post_lc_read_valid=true;
    return (rtl_passive.post_lc_channel&RTL_CU_RF_DATA_MASK)==RTL_CU_RF_START_CHANNEL;
}

static bool rtl_cu_radio_prepare(struct xhci_device *d,struct xhci_device *hub,
                                 const uint8_t *blob,unsigned *writes,uint32_t *channel) {
    uint32_t state;
    if(!rtl_cu_tx_parked(d) ||
       !rtl8188cu_update(d,RTL8188CU_ISOLATION,2,1u<<9,0) ||
       !rtl8188cu_update(d,0x600,1,1u<<6,0) ||
       !rtl8188cu_poll(d,0x600,1,1u<<7,0,32,&state) ||
       !rtl8188cu_update(d,RTL8188CU_SYS_FUNC,2,0,0x2003) ||
       !rtl8188cu_write_value(d,0x28,1,0x83) || !rtl8188cu_write_value(d,0x29,1,0xdb) ||
       !rtl8188cu_write_value(d,RTL8188CU_RF_CONTROL,1,7) ||
       !rtl8188cu_write_value(d,RTL8188CU_SYS_FUNC,1,0x17) ||
       !rtl8188cu_update(d,0x87c,4,1u<<31,0) ||
       !rtl8188cu_write_value(d,0x22,1,0x0f) ||
       !rtl8188cu_write_value(d,0x25,1,0x80))return false;
    rtl_passive.profile_started=true;
    uint64_t start=timer_monotonic_us();
    for(unsigned i=0;i<RTL_CU_PROFILE_WRITES;i++) {
        const uint8_t *row=blob+6*i;
        if(timer_monotonic_us()-start>=3000000 ||
           ((i&63)==0 && !rtl8188cu_check_port(d,hub)) ||
           !rtl8188cu_write_value(d,row[0]|(unsigned)row[1]<<8,4,rtl_cu_le32(row+2)))return false;
        (*writes)++;
        uint32_t delay=rtl_cu_profile_wait(i);
        if(delay) {
            uint64_t before=timer_monotonic_us();
            if(!timer_delay_us(delay))return false;
            rtl_passive.profile_wait_us+=timer_monotonic_us()-before;
            rtl_passive.profile_waits++;
        }
    }
    if(!rtl8188cu_check_port(d,hub) ||
       !rtl_cu_rf_read(d,0x18,&rtl_passive.startup_rf_channel))return false;
    rtl_passive.startup_rf_valid=true;
    if((rtl_passive.startup_rf_channel&RTL_CU_RF_DATA_MASK)!=RTL_CU_RF_START_CHANNEL ||
       !rtl_cu_post_profile_lc(d,hub))return false;
    /* Verify the channel through RF path A and keep both aggregation paths
     * off so a single receive buffer describes one frame. */
    if(!rtl8188cu_update(d,0x603,1,0,4) ||
       !rtl8188cu_update(d,0x800,4,1,0) ||
       !rtl8188cu_update(d,0x900,4,1,0) ||
       !rtl8188cu_write_value(d,0x840,4,0x0180740a) ||
       !rtl_cu_rf_read(d,0x18,channel) || (*channel&0x3ff)!=RTL_CU_CHANNEL ||
       !rtl8188cu_update(d,0x10c,1,4,0) || !rtl8188cu_update(d,0xfe55,1,8,0) ||
       !rtl8188cu_write_value(d,0x60f,1,0) ||
       !rtl8188cu_write_value(d,0x608,4,RTL_CU_MGMT_RCR) ||
       !rtl8188cu_write_value(d,0x6a0,2,RTL_CU_MGMT_MAP) ||
       !rtl8188cu_write_value(d,0x6a2,2,0) || !rtl8188cu_write_value(d,0x6a4,2,0))return false;
    if(!rtl8188cu_read(d,0xc50,4,&rtl_passive.gain[0]))return false;
    rtl_passive.gain_valid=1;
    if(!rtl8188cu_read(d,0xc58,4,&rtl_passive.gain[1]))return false;
    rtl_passive.gain_valid=3;
    return rtl8188cu_check_port(d,hub) && rtl_cu_tx_parked(d);
}

static bool rtl_cu_rx_configure(struct xhci_device *d) {
    void *page;
    struct xhci_bulk_endpoint *ep=&rtl_passive.ep;
    if(!alloc_frame_zero(&ep->ring_phys,&page))return false;
    ep->ring=page;ep->cycle=true;ep->packet=512;ep->dci=RTL_CU_RX_DCI;
    write_link_trb(ep->ring,ep->ring_phys,true);
    if(!alloc_frame_zero(&rtl_passive.physical,&page))return false;
    rtl_passive.buffer=page;
    zero_bytes(d->input_context,PMM_FRAME_SIZE);
    context_write32(d->input_context,0,1,1u|(1u<<RTL_CU_RX_DCI));
    uint32_t slot=context_read32(d->output_context,0,0);
    context_write32(d->input_context,1,0,(slot&~(31u<<27))|(RTL_CU_RX_DCI<<27));
    for(unsigned i=1;i<3;i++)context_write32(d->input_context,1,i,context_read32(d->output_context,0,i));
    const unsigned idx=RTL_CU_RX_DCI+1;
    context_write32(d->input_context,idx,1,(3u<<1)|(6u<<3)|(512u<<16));
    context_write32(d->input_context,idx,2,(uint32_t)(ep->ring_phys|1));
    context_write32(d->input_context,idx,3,ep->ring_phys>>32);
    context_write32(d->input_context,idx,4,RTL_CU_RX_BYTES);
    const struct xhci_trb cmd={.parameter_low=(uint32_t)d->input_context_phys,.parameter_high=d->input_context_phys>>32,
        .control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)d->slot_id<<24)};
    uint64_t completed=UINT64_MAX;
    rtl_passive.configured=submit_command_trb(&cmd,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,&completed) &&
        completed==d->slot_id;
    if(completed!=UINT64_MAX && completed!=d->slot_id)xhci.command_failed=true;
    return rtl_passive.configured;
}

static bool rtl8188cu_passive_event(const struct xhci_device *d,uint8_t dci,uint64_t pointer,
                                    uint8_t completion,uint32_t remaining,bool event_data) {
    if(!d || d!=rtl_passive.owner || dci!=RTL_CU_RX_DCI || !rtl_passive.configured)return false;
    rtl_passive.events++;
    rtl_passive.event_trb=pointer;
    rtl_passive.event_code=completion;
    rtl_passive.event_residue=remaining;
    rtl_passive.event_data=event_data;
    if(pointer!=rtl_passive.trb)return false;
    if(!rtl_passive.pending)return true;
    if(event_data || (completion!=1 && completion!=13) || remaining>RTL_CU_RX_BYTES) {
        rtl_passive.failed=true;
        return true;
    }
    rtl_passive.actual=RTL_CU_RX_BYTES-remaining;
    if(!rtl_passive.completed)rtl_passive.first_completion_us=timer_monotonic_us()-rtl_passive.started_us;
    rtl_passive.pending=false;rtl_passive.completed++;
    return true;
}

static void rtl_cu_rx_post(struct xhci_device *d) {
    struct xhci_bulk_endpoint *ep=&rtl_passive.ep;
    const struct xhci_trb transfer={.parameter_low=(uint32_t)rtl_passive.physical,
        .parameter_high=rtl_passive.physical>>32,.status=RTL_CU_RX_BYTES,
        .control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    rtl_passive.trb=ring_push(ep->ring,ep->ring_phys,&ep->enqueue,&ep->cycle,&transfer);
    rtl_passive.actual=0;rtl_passive.pending=true;rtl_passive.posted++;
    db_write32(d->slot_id,RTL_CU_RX_DCI);
}

static void rtl_cu_rx_inspect(const uint8_t *data,size_t length) {
    static const uint8_t wanted[]="Demers2.4";
    if(length<24){rtl_passive.rejected++;return;}
    uint32_t word=rtl_cu_le32(data);
    size_t offset=24+8*((word>>16)&15)+((word>>24)&3);
    size_t mpdu=word&0x3fff;
    if((word&0xc000) || offset>length || mpdu>length-offset) {
        rtl_passive.rejected++;return;
    }
    struct mt76_rx_beacon beacon;
    if(wifi_rx_beacon_frame(data+offset,mpdu,RTL_CU_CHANNEL,&beacon)!=MT76_RX_OK)return;
    rtl_passive.beacons++;
    if(beacon.ssid_size!=sizeof wanted-1 || !beacon.ds_present)return;
    for(unsigned i=0;i<sizeof wanted-1;i++)if(beacon.ssid[i]!=wanted[i])return;
    rtl_passive.matches++;
}

static bool rtl_cu_listen(struct xhci_device *d,struct xhci_device *hub) {
    if(!rtl_cu_tx_parked(d) || !rtl8188cu_update(d,RTL8188CU_MAC_CONTROL,1,0,RTL_CU_RX_ENABLE) ||
       !rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&rtl_passive.rx_cr))return false;
    rtl_passive.rx_cr_valid=true;
    if((rtl_passive.rx_cr&(RTL_CU_RX_ENABLE|RTL_CU_TX_GATES))!=RTL_CU_RX_ENABLE)return false;
    rtl_cu_rx_snapshot(d,RTL_CU_ARMED,true);
    if(d->control_failed || d->ep0_quarantined || device_removed(d))return false;
    console_write("rtl8188cu-passive-progress: listening on channel 10; no input required\n");
    uint64_t start=timer_monotonic_us(),last=start,clock=start,stalled=0;
    rtl_passive.started_us=start;
    rtl_cu_rx_post(d);
    for(uint64_t spins=0;;spins++) {
        struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
        if(rtl_passive.failed || device_removed(d))return false;
        if(!rtl_passive.pending) {
            rtl_cu_rx_inspect(rtl_passive.buffer,rtl_passive.actual);
            zero_bytes(rtl_passive.buffer,RTL_CU_RX_BYTES);
            if(rtl_passive.matches>=2 || rtl_passive.posted==62)return true;
            rtl_cu_rx_post(d);
        }
        if((spins&255)==0) {
            uint64_t now=timer_monotonic_us();
            if(now-start>=4000000)return true;
            stalled=now==clock?stalled+256:0;
            clock=now;
            if(stalled>=XHCI_TRANSFER_TIMEOUT)return false;
            if(now-last>=250000) {
                if(!rtl8188cu_check_port(d,hub) || !rtl_cu_tx_parked(d))return false;
                last=now;
                if(!rtl_passive.completed && !rtl_passive.samples[RTL_CU_PENDING].present) {
                    rtl_cu_rx_snapshot(d,RTL_CU_PENDING,true);
                    if(d->control_failed || d->ep0_quarantined)return false;
                }
            }
        }
        __asm__ volatile("pause");
    }
}

/* Keep the ring and buffer pinned until reboot, including failed stops. */
static bool rtl_cu_rx_stop(struct xhci_device *d) {
    uint64_t slot=UINT64_MAX;
    bool ok=submit_command(XHCI_TRB_TYPE_STOP_ENDPOINT,
       ((uint32_t)d->slot_id<<24)|(RTL_CU_RX_DCI<<16),&slot);
    if(slot!=UINT64_MAX && slot!=d->slot_id)xhci.command_failed=true;
    if(!ok || slot!=d->slot_id)return false;
    uint64_t dequeue=rtl_cu_rx_dequeue(d);
    uint64_t cursor=dequeue&~15ull;
    bool stopped=(context_read32(d->output_context,RTL_CU_RX_DCI,0)&7)==3 &&
        cursor>=rtl_passive.ep.ring_phys && cursor-rtl_passive.ep.ring_phys<1024 &&
        (dequeue&15)==1;
    if(stopped){rtl_passive.pending=false;zero_bytes(rtl_passive.buffer,RTL_CU_RX_BYTES);}
    return stopped;
}

bool xhci_rtl8188cu_capture_passive(const uint8_t *blob,uint64_t size) {
    const char *result="admission";
    unsigned writes=0,linked=0;
    uint32_t channel=0,mac=0,rf=0;
    bool buffer_ready=false,stopped=false,parked=false,window=false;
    uint64_t flags=event_irq_save();
    for(unsigned i=0;i<XHCI_MAX_DEVICES;i++) {
        struct xhci_device *d=&xhci.devices[i];
        if(!d->rtl8188cu_firmware_ready)continue;
        struct xhci_device *hub=device_by_slot(d->parent_slot_id);
        if(rtl_passive.owner || !hub || !wifi_hub_path_present(d) || !timer_reference_ready() ||
           xhci.sync_users || strict_control.device || !rtl_cu_profile_valid(blob,size))break;
        rtl_passive.owner=d;
        d->rtl8188cu_firmware_active=true;
        bool touched=rtl8188cu_check_port(d,hub) && rtl8188cu_radio_parked(d);
        result="packet-buffer";
        buffer_ready=touched && rtl8188cu_write_value(d,RTL_CU_TX_PAUSE,1,0xff) &&
            rtl_cu_packet_buffer(d,hub,&linked);
        if(buffer_ready) {
            result="setup";
            if(rtl_cu_radio_prepare(d,hub,blob,&writes,&channel)) {
                bool rx_setup=rtl_cu_verify_receive_setup(d);
                result=rx_setup?"rx-endpoint":"rx-setup";
                if(rx_setup && rtl_cu_rx_configure(d)) {
                    window=rtl_cu_listen(d,hub);
                    rtl_cu_rx_snapshot(d,RTL_CU_BEFORE_STOP,window);
                    window=window && !rtl_passive.failed && !d->control_failed && !d->ep0_quarantined;
                    stopped=rtl_cu_rx_stop(d);
                    rtl_cu_rx_snapshot(d,RTL_CU_AFTER_STOP,false);
                    result=window && !rtl_passive.completed?"rx-timeout":"window";
                }
            }
        }
        if(touched && !d->control_failed && !d->ep0_quarantined && rtl8188cu_check_port(d,hub)) {
            bool rx_off=rtl8188cu_update(d,RTL8188CU_MAC_CONTROL,1,RTL_CU_RX_ENABLE,0);
            bool rf_off=rtl8188cu_update(d,RTL8188CU_RF_CONTROL,1,RTL8188CU_RF_ENABLE,0);
            parked=rx_off && rf_off && rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&mac) &&
                rtl8188cu_read(d,RTL8188CU_RF_CONTROL,1,&rf) &&
                !(mac&(RTL_CU_RX_ENABLE|RTL_CU_TX_GATES)) && !(rf&RTL8188CU_RF_ENABLE);
        }
        d->rtl8188cu_firmware_active=false;
        if(window && stopped && parked && rtl_passive.completed)
            result=rtl_passive.matches>=2?"target":"target-absent";
        break;
    }
    event_irq_restore(flags);
    rtl_cu_rx_log();
    if(rtl_passive.owner)
        console_printf("rtl8188cu-packet-buffer: ready=%u linked=%u first_completion_us=%u\n",
            (uint64_t)buffer_ready,(uint64_t)linked,rtl_passive.first_completion_us);
    console_printf("rtl8188cu-passive: result=%s writes=%u rf_channel=%x posted=%u completed=%u beacons=%u target=%u rejected=%u stopped=%u parked=%u\n",
        result,(uint64_t)writes,(uint64_t)channel,(uint64_t)rtl_passive.posted,(uint64_t)rtl_passive.completed,
        (uint64_t)rtl_passive.beacons,(uint64_t)rtl_passive.matches,(uint64_t)rtl_passive.rejected,(uint64_t)stopped,(uint64_t)parked);
    return window && stopped && parked && rtl_passive.matches>=2;
}
