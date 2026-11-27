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
/* Match Linux monitor admission and enable its PHY status prefix. */
#define RTL_CU_MONITOR_RCR 0x790fu
#define RTL_CU_RCR_PHY_STATUS (1u<<28)
#define RTL_CU_RX_DRIVER_INFO_UNITS 4u
#define RTL_CU_ALL_SUBTYPES 0xffffu
#define RTL_CU_RF_DATA_MASK 0xfffffu
#define RTL_CU_RF_START_CHANNEL 0x7401u
#define RTL_CU_RF_PATH_SELECT 0x878u
#define RTL_CU_LED_CONFIG 0x04cu
#define RTL_CU_RF_PATH_MUX (1u<<23)
#define RTL_CU_RF0_LINUX_WRITE 0x32ed3u
/* The working Linux RF0 readback varies below bit 16. */
#define RTL_CU_RF0_STABLE_MASK 0xf0000u
#define RTL_CU_RF0_STABLE_RX 0x30000u
#define RTL_CU_LC_START 0x8000u
#define RTL_CU_RX_REPORT 0x0664u
#define RTL_CU_REPORT_SELECTOR_MASK 0xf0000000u
#define RTL_CU_REPORT_COUNT_MASK 0xfffffu
#define RTL_CU_RX_IQ 0xc14u
#define RTL_CU_RX_IQ_EXT 0xca0u
#define RTL_CU_IQ_STARTUP_B68 0xb68u
#define RTL_CU_IQ_STARTUP_B68_LINUX 0x00080000u
#define RTL_CU_SPEC_SIFS 0x428u
#define RTL_CU_SPEC_SIFS_LINUX 0x0a10u
#define RTL_CU_IQ_MODE 0xe28u
#define RTL_CU_IQ_CAL_CONTROL 0xe4cu
#define RTL_CU_IQ_CAL_CONTROL_LINUX (1u<<20)
#define RTL_CU_TRXDMA_CONTROL 0x10cu
#define RTL_CU_TRXDMA_RX_BITS 7u
#define RTL_CU_USB_OPTION 0xfe55u
#define RTL_CU_USB_RX_AGG (1u<<3)
#define RTL_CU_APS_RADIO_GPIO_LED 0x0812u
#define RTL_CU_AFE_XTAL_CONTROL 0x024u
#define RTL_CU_AFE_XTAL_LINUX 0x0011800fu
enum rtl_cu_iq_field {
    RTL_CU_IQ_RX,RTL_CU_IQ_RX_EXT,RTL_CU_IQ_TX,RTL_CU_IQ_TX_EXT,RTL_CU_IQ_TX_CCA,RTL_CU_IQ_FIELDS
};

enum rtl_cu_sample_phase {
    RTL_CU_ARMED,RTL_CU_PENDING,RTL_CU_BEFORE_STOP,RTL_CU_AFTER_STOP,RTL_CU_SAMPLE_COUNT
};
enum rtl_cu_rx_register { RTL_CU_CR,RTL_CU_RCR,RTL_CU_RXFF_PTR,RTL_CU_RXPKT_NUM,RTL_CU_RXDMA_STATUS,RTL_CU_RX_REG_COUNT };
enum rtl_cu_phy_register { RTL_CU_OFDM1,RTL_CU_OFDM2,RTL_CU_OFDM3,RTL_CU_SYNC,RTL_CU_PHY_COUNT };
enum rtl_cu_report {
    RTL_CU_OFDM_OK,RTL_CU_CCK_OK,RTL_CU_HT_OK,RTL_CU_FIFO_DROP,
    RTL_CU_OFDM_PPDU,RTL_CU_OFDM_FALSE,RTL_CU_OFDM_FAIL,
    RTL_CU_CCK_PPDU,RTL_CU_CCK_FALSE,RTL_CU_CCK_FAIL,
    RTL_CU_HT_PPDU,RTL_CU_HT_FALSE,RTL_CU_HT_TOTAL,RTL_CU_HT_FAIL,
    RTL_CU_REPORT_COUNT
};
enum rtl_cu_rx_admission {
    RTL_CU_FILTER_RCR,RTL_CU_FILTER_MGT,RTL_CU_FILTER_CTL,RTL_CU_FILTER_DATA,RTL_CU_DRIVER_INFO,
    RTL_CU_FPGA0_RFMOD,RTL_CU_FPGA1_RFMOD,RTL_CU_OFDM_TRX,RTL_CU_MAR0,RTL_CU_MAR1,RTL_CU_ADMISSION_COUNT
};
enum rtl_cu_rf0_phase {
    RTL_CU_RF0_BEFORE_WRITE,RTL_CU_RF0_PRE_LC,RTL_CU_RF0_POST_LC,RTL_CU_RF0_TUNED,RTL_CU_RF0_COUNT
};
/* Compare the Linux monitor's RF control, channel width, path routing, gain,
 * and IQ matrix. These reads require neither an IQK run nor a MAC TX gate. */
static const struct { uint16_t address,width; } rtl_cu_phy_radio_regs[]={
    {0x01f,1},{0x603,1},{0x800,4},{0x804,4},{0x860,4},{0x864,4},{0x870,4},
    {0x874,4},{RTL_CU_RF_PATH_SELECT,4},{0x87c,4},{0x884,4},{0x900,4},
    {0xa00,4},{0xa04,4},{0xa08,4},{0xa0c,4},{0xc04,4},{0xc08,4},{0xc14,4},
    {0xc4c,4},{0xc50,4},{0xc58,4},{0xc70,4},{0xc80,4},{0xc94,4},{0xca0,4},
    {0xd00,4},{0xd04,4},
};
#define RTL_CU_PHY_RADIO_COUNT (sizeof rtl_cu_phy_radio_regs/sizeof rtl_cu_phy_radio_regs[0])
static const uint16_t rtl_cu_iq_startup_regs[]={
    RTL_CU_LED_CONFIG,RTL_CU_SPEC_SIFS,RTL_CU_IQ_STARTUP_B68,RTL_CU_IQ_MODE,0xe48,RTL_CU_IQ_CAL_CONTROL
};
#define RTL_CU_IQ_STARTUP_COUNT (sizeof rtl_cu_iq_startup_regs/sizeof rtl_cu_iq_startup_regs[0])
enum rtl_cu_dma_register { RTL_CU_DMA_TRXDMA,RTL_CU_DMA_USB_OPTION };
static const struct { uint16_t address,width; } rtl_cu_dma_regs[]={
    [RTL_CU_DMA_TRXDMA]={RTL_CU_TRXDMA_CONTROL,2},
    [RTL_CU_DMA_USB_OPTION]={RTL_CU_USB_OPTION,1},
    {0x104,1},{0x114,1},{0x116,2},{0x118,4},{0x280,2},{0xfe5b,1},{0xfe5c,1},{0xfe5d,1},
};
#define RTL_CU_DMA_COUNT (sizeof rtl_cu_dma_regs/sizeof rtl_cu_dma_regs[0])
enum rtl_cu_enable_register { RTL_CU_ENABLE_XTAL=7 };
static const struct { uint16_t address,width; } rtl_cu_enable_regs[]={
    {0x000,2},{0x002,2},{RTL8188CU_POWER,2},{0x008,2},{0x010,1},{0x015,1},
    {0x021,1},[RTL_CU_ENABLE_XTAL]={RTL_CU_AFE_XTAL_CONTROL,4},
    {0x028,4},{RTL8188CU_MAC_CONTROL,4},{0x600,1},{0x604,4},
    {0x608,4},{0x60c,1},{0x60d,1},{0x60f,1},{0x668,4},{0x692,1},
    {0x6b4,4},{0x800,4},{0x900,4},{0xfe40,1},{0xfe41,1},{0xfe42,1},
};
#define RTL_CU_ENABLE_COUNT (sizeof rtl_cu_enable_regs/sizeof rtl_cu_enable_regs[0])
struct rtl_cu_rx_sample {
    uint64_t start_us,end_us,dequeue,trb;
    uint32_t regs[RTL_CU_RX_REG_COUNT];
    uint32_t phy[RTL_CU_PHY_COUNT];
    uint32_t report[RTL_CU_REPORT_COUNT];
    unsigned state,valid,phy_valid,posted,completed_before,completed_after,events;
    unsigned report_valid;
    bool report_restored;
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
    uint32_t early_rx_before,early_rx_after;
    uint64_t early_rx_us,llt_started_us;
    unsigned early_rx_reads,linked;
    bool buffer_ready,parked;
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
    uint32_t phy_radio[RTL_CU_PHY_RADIO_COUNT],rf0[RTL_CU_RF0_COUNT];
    unsigned phy_radio_reads,rf0_valid;
    bool rf0_write_ok,phy_radio_ready;
    uint32_t ledcfg_before,ledcfg_after;
    unsigned ledcfg_valid;
    uint32_t iq_before[RTL_CU_IQ_FIELDS],iq_after[RTL_CU_IQ_FIELDS],iq_startup[RTL_CU_IQ_STARTUP_COUNT];
    unsigned iq_valid,iq_startup_reads;
    bool iq_attempted,iq_ready;
    uint32_t b68_readback,sifs_readback,e4c_readback;
    bool b68_attempted,b68_valid,sifs_valid,e4c_valid;
    uint32_t dma_regs[RTL_CU_DMA_COUNT],trxdma_before,usb_opt_before;
    unsigned dma_before_valid,dma_reads;
    bool dma_attempted,dma_ready;
    uint32_t aps_before,aps_after,enable_regs[RTL_CU_ENABLE_COUNT];
    unsigned aps_reads,enable_reads;
    bool aps_attempted,aps_ready;
    uint32_t xtal_before,xtal_after;
    unsigned xtal_reads;
    bool xtal_attempted,xtal_ready;
    bool event_data,rx_cr_valid;
    bool configured,pending,failed;
} rtl_passive;

static bool rtl_cu_verify_receive_setup(struct xhci_device *d) {
    /* Verify the frame filters and pinned baseband layout. */
    static const struct { uint16_t address,width; uint32_t expected; } regs[RTL_CU_ADMISSION_COUNT]={
        [RTL_CU_FILTER_RCR]={0x608,4,RTL_CU_MONITOR_RCR|RTL_CU_RCR_PHY_STATUS},
        [RTL_CU_FILTER_MGT]={0x6a0,2,RTL_CU_ALL_SUBTYPES},
        [RTL_CU_FILTER_CTL]={0x6a2,2,0}, [RTL_CU_FILTER_DATA]={0x6a4,2,RTL_CU_ALL_SUBTYPES},
        [RTL_CU_DRIVER_INFO]={0x60f,1,RTL_CU_RX_DRIVER_INFO_UNITS},
        [RTL_CU_FPGA0_RFMOD]={0x800,4,0x83040000}, [RTL_CU_FPGA1_RFMOD]={0x900,4,0},
        [RTL_CU_OFDM_TRX]={0xc04,4,0x03a05611},
        [RTL_CU_MAR0]={0x620,4,UINT32_MAX}, [RTL_CU_MAR1]={0x624,4,UINT32_MAX},
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

static void rtl_cu_rx_reports(struct xhci_device *d,struct rtl_cu_rx_sample *sample) {
    /* Select the CU report without writing the counter or the reset bit. */
    static const unsigned report_selector[RTL_CU_REPORT_COUNT]={
        [RTL_CU_OFDM_OK]=2, [RTL_CU_CCK_OK]=6, [RTL_CU_HT_OK]=11, [RTL_CU_FIFO_DROP]=15,
        [RTL_CU_OFDM_PPDU]=0, [RTL_CU_OFDM_FALSE]=1, [RTL_CU_OFDM_FAIL]=3,
        [RTL_CU_CCK_PPDU]=4, [RTL_CU_CCK_FALSE]=5, [RTL_CU_CCK_FAIL]=7,
        [RTL_CU_HT_PPDU]=8, [RTL_CU_HT_FALSE]=9, [RTL_CU_HT_TOTAL]=10, [RTL_CU_HT_FAIL]=12,
    };
    uint32_t initial,value;
    if(!rtl8188cu_read(d,RTL_CU_RX_REPORT,4,&initial))return;
    initial&=RTL_CU_REPORT_SELECTOR_MASK;
    for(unsigned i=0;i<RTL_CU_REPORT_COUNT;i++) {
        uint32_t selector=report_selector[i]<<28;
        if(!rtl8188cu_write_value(d,RTL_CU_RX_REPORT,4,selector) ||
           !rtl8188cu_read(d,RTL_CU_RX_REPORT,4,&value) || value>>28!=report_selector[i])break;
        sample->report[i]=value;
        sample->report_valid|=1u<<i;
    }
    if(!device_removed(d) && !d->control_failed && !d->ep0_quarantined &&
       rtl8188cu_write_value(d,RTL_CU_RX_REPORT,4,initial) &&
       rtl8188cu_read(d,RTL_CU_RX_REPORT,4,&value) &&
       (value&RTL_CU_REPORT_SELECTOR_MASK)==initial)sample->report_restored=true;
}

static void rtl_cu_rx_snapshot(struct xhci_device *d,enum rtl_cu_sample_phase phase,bool read_chip) {
    /* These USB vendor requests read status only. The endpoint context comes
     * first, and completions may advance while EP0 reads are in flight. */
    static const struct { uint16_t address,width; } regs[RTL_CU_RX_REG_COUNT]={
        [RTL_CU_CR]={RTL8188CU_MAC_CONTROL,2}, [RTL_CU_RCR]={0x608,4},
        [RTL_CU_RXFF_PTR]={0x11c,4}, [RTL_CU_RXPKT_NUM]={0x284,4}, [RTL_CU_RXDMA_STATUS]={0x288,4},
    };
    /* The 92C Linux driver reads these OFDM false-alarm and synchronization
     * registers before its reset sequence. A diagnostic only reads them. */
    static const uint16_t phy_regs[RTL_CU_PHY_COUNT]={
        [RTL_CU_OFDM1]=0xda0, [RTL_CU_OFDM2]=0xda4, [RTL_CU_OFDM3]=0xda8, [RTL_CU_SYNC]=0xcf0,
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
        if(sample->valid==(1u<<RTL_CU_RX_REG_COUNT)-1 && !rtl_passive.failed) {
            for(unsigned i=0;i<RTL_CU_PHY_COUNT;i++) {
                if(!rtl8188cu_read(d,phy_regs[i],4,&sample->phy[i]))break;
                sample->phy_valid|=1u<<i;
            }
        }
        if(sample->phy_valid==(1u<<RTL_CU_PHY_COUNT)-1 && !rtl_passive.failed)
            rtl_cu_rx_reports(d,sample);
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
        console_printf("rtl8188cu-rx-setup: ready=%u valid=%x rcr=%x mgt=%x ctl=%x data=%x drvinfo=%x fpga0=%x fpga1=%x ofdm_trx=%x mar0=%x mar1=%x\n",
            (uint64_t)rtl_passive.admission_ok,(uint64_t)rtl_passive.admission_valid,
            (uint64_t)r[RTL_CU_FILTER_RCR],(uint64_t)r[RTL_CU_FILTER_MGT],(uint64_t)r[RTL_CU_FILTER_CTL],
            (uint64_t)r[RTL_CU_FILTER_DATA],(uint64_t)r[RTL_CU_DRIVER_INFO],
            (uint64_t)r[RTL_CU_FPGA0_RFMOD],(uint64_t)r[RTL_CU_FPGA1_RFMOD],(uint64_t)r[RTL_CU_OFDM_TRX],
            (uint64_t)r[RTL_CU_MAR0],(uint64_t)r[RTL_CU_MAR1]);
    if(rtl_passive.startup_rf_valid)
        console_printf("rtl8188cu-rf-setup: ready=%u valid=%x write_ok=%u before=%x pre_lc=%x post_lc=%x tuned=%x reads=%u mux_valid=%u path_mux=%x\n",
            (uint64_t)rtl_passive.phy_radio_ready,(uint64_t)rtl_passive.rf0_valid,(uint64_t)rtl_passive.rf0_write_ok,
            (uint64_t)rtl_passive.rf0[RTL_CU_RF0_BEFORE_WRITE],(uint64_t)rtl_passive.rf0[RTL_CU_RF0_PRE_LC],
            (uint64_t)rtl_passive.rf0[RTL_CU_RF0_POST_LC],(uint64_t)rtl_passive.rf0[RTL_CU_RF0_TUNED],
            (uint64_t)rtl_passive.phy_radio_reads,(uint64_t)rtl_passive.ledcfg_valid,
            ((uint64_t)rtl_passive.ledcfg_before<<32)|rtl_passive.ledcfg_after);
    if(rtl_passive.phy_radio_reads) {
        /* Pack (address << 32) | value to fit the independent journal tail. */
        console_write("rtl8188cu-phy-radio:");
        for(unsigned i=0;i<rtl_passive.phy_radio_reads;i++)
            console_printf(" %x",((uint64_t)rtl_cu_phy_radio_regs[i].address<<32)|rtl_passive.phy_radio[i]);
        console_write("\n");
    }
    if(rtl_passive.iq_attempted) {
        console_printf("rtl8188cu-iq-matrix: ready=%u valid=%x",
            (uint64_t)rtl_passive.iq_ready,(uint64_t)rtl_passive.iq_valid);
        for(unsigned i=0;i<RTL_CU_IQ_FIELDS;i++)
            console_printf(" %x",((uint64_t)rtl_passive.iq_before[i]<<32)|rtl_passive.iq_after[i]);
        console_write("\n");
    }
    if(rtl_passive.iq_startup_reads) {
        console_write("rtl8188cu-iq-startup:");
        for(unsigned i=0;i<rtl_passive.iq_startup_reads;i++)
            console_printf(" %x",((uint64_t)rtl_cu_iq_startup_regs[i]<<32)|rtl_passive.iq_startup[i]);
        console_write("\n");
    }
    if(rtl_passive.iq_startup_reads==RTL_CU_IQ_STARTUP_COUNT)
        console_printf("rtl8188cu-init-write: %u %x %x\n",
            (uint64_t)(rtl_passive.sifs_valid|(rtl_passive.b68_valid<<1)|(rtl_passive.e4c_valid<<2)),
            ((uint64_t)rtl_passive.sifs_readback<<32)|rtl_passive.b68_readback,
            (uint64_t)rtl_passive.e4c_readback);
    if(rtl_passive.dma_attempted) {
        console_printf("rtl8188cu-rx-dma: %u %u %x",(uint64_t)rtl_passive.dma_ready,
            (uint64_t)rtl_passive.dma_before_valid,((uint64_t)rtl_passive.trxdma_before<<32)|rtl_passive.usb_opt_before);
        for(unsigned i=0;i<rtl_passive.dma_reads;i++)
            console_printf(" %x",((uint64_t)rtl_cu_dma_regs[i].address<<32)|rtl_passive.dma_regs[i]);
        console_write("\n");
    }
    if(rtl_passive.aps_attempted) {
        console_printf("rtl8188cu-rx-enable: %u %u %u %x",(uint64_t)rtl_passive.aps_ready,
            (uint64_t)rtl_passive.aps_reads,(uint64_t)rtl_passive.enable_reads,
            ((uint64_t)rtl_passive.aps_before<<32)|rtl_passive.aps_after);
        for(unsigned i=0;i<rtl_passive.enable_reads;i+=2) {
            uint32_t next=i+1<rtl_passive.enable_reads?rtl_passive.enable_regs[i+1]:0;
            console_printf(" %x",((uint64_t)rtl_passive.enable_regs[i]<<32)|next);
        }
        console_write("\n");
    }
    if(rtl_passive.xtal_attempted)
        console_printf("rtl8188cu-xtal: %u %u %x\n",(uint64_t)rtl_passive.xtal_ready,
            (uint64_t)rtl_passive.xtal_reads,((uint64_t)rtl_passive.xtal_before<<32)|rtl_passive.xtal_after);
    rtl8188cu_power_log(rtl_passive.owner,"passive");
    if(rtl_passive.early_rx_reads)
        console_printf("rtl8188cu-rx-timing: %u %x %u %u\n",(uint64_t)rtl_passive.early_rx_reads,
            ((uint64_t)rtl_passive.early_rx_before<<32)|rtl_passive.early_rx_after,
            rtl_passive.early_rx_us,rtl_passive.started_us);
    if(rtl_passive.owner)
        console_printf("rtl8188cu-startup-order: %u %u\n",rtl_passive.llt_started_us,
            rtl_passive.owner->rtl8188cu_download_started_us);
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
        if(i!=RTL_CU_AFTER_STOP)
            console_printf("rtl8188cu-rx-phy: phase=%s valid=%x ofdm1=%x ofdm2=%x ofdm3=%x sync=%x\n",
                phases[i],(uint64_t)s->phy_valid,(uint64_t)s->phy[RTL_CU_OFDM1],(uint64_t)s->phy[RTL_CU_OFDM2],
                (uint64_t)s->phy[RTL_CU_OFDM3],(uint64_t)s->phy[RTL_CU_SYNC]);
        if(i!=RTL_CU_AFTER_STOP) {
            /* Pack three 20-bit counters per word to fit the EFI journal.
             * The validity mask distinguishes a missing read from zero. */
            console_printf("rtl8188cu-rx-decode: %s %u %u",phases[i],
                (uint64_t)s->report_valid,(uint64_t)s->report_restored);
            for(unsigned j=0;j<RTL_CU_REPORT_COUNT;j+=3) {
                uint64_t word=0;
                for(unsigned k=0;k<3 && j+k<RTL_CU_REPORT_COUNT;k++)
                    word|=(uint64_t)(s->report[j+k]&RTL_CU_REPORT_COUNT_MASK)<<(20*k);
                console_printf(" %x",word);
            }
            console_write("\n");
        }
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

static bool rtl_cu_open_rx_early(struct xhci_device *d) {
    /* APSD is ready and RF is still off. Open the receive path before LLT. */
    uint32_t rf;
    if(!rtl_cu_tx_parked(d) || !rtl8188cu_read(d,RTL8188CU_RF_CONTROL,1,&rf) ||
       (rf&RTL8188CU_RF_ENABLE) || !rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&rtl_passive.early_rx_before))
        return false;
    rtl_passive.early_rx_reads=1;
    if(rtl_passive.early_rx_before || !rtl8188cu_write_value(d,RTL8188CU_MAC_CONTROL,1,RTL_CU_RX_ENABLE) ||
       !rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&rtl_passive.early_rx_after))return false;
    rtl_passive.early_rx_reads=2;
    if(rtl_passive.early_rx_after!=RTL_CU_RX_ENABLE)return false;
    rtl_passive.early_rx_us=timer_monotonic_us();
    return true;
}

static bool rtl_cu_rx_held(struct xhci_device *d) {
    uint32_t mac,pause;
    return rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&mac) && mac==RTL_CU_RX_ENABLE &&
        rtl8188cu_read(d,RTL_CU_TX_PAUSE,1,&pause) && pause==0xff;
}

/* The CU packet buffer needs its page links even with MAC TX and TX DMA
 * closed. Preserve the captured layout and bounds with RX already armed. */
static bool rtl_cu_packet_buffer(struct xhci_device *d,struct xhci_device *hub) {
    uint32_t value;
    const uint64_t start=timer_monotonic_us();
    for(unsigned page=0;page<256;page++) {
        unsigned next=page+1;
        if(page==RTL_CU_PACKET_BOUNDARY-1)next=0xff;
        if(page==0xff)next=RTL_CU_PACKET_BOUNDARY;
        if(timer_monotonic_us()-start>=1000000 ||
           ((page&63)==0 && (!rtl8188cu_check_port(d,hub) || !rtl_cu_rx_held(d))))return false;
        if(!page)rtl_passive.llt_started_us=timer_monotonic_us();
        if(!rtl8188cu_write_value(d,RTL_CU_LLT,4,RTL_CU_LLT_WRITE|(page<<8)|next) ||
           !rtl8188cu_poll(d,RTL_CU_LLT,4,RTL_CU_LLT_BUSY,0,8,&value) ||
           ((value>>8)&0xff)!=page)return false;
        rtl_passive.linked++;
    }
    return rtl8188cu_write_value(d,0x114,1,RTL_CU_PACKET_BOUNDARY) &&
           rtl8188cu_write_value(d,0x116,2,0x27ff) &&
           rtl8188cu_write_value(d,0x104,1,0x11) &&
           rtl8188cu_read(d,0x114,1,&value) && value==RTL_CU_PACKET_BOUNDARY &&
           rtl8188cu_read(d,0x116,2,&value) && value==0x27ff &&
           rtl8188cu_read(d,0x104,1,&value) && value==0x11 && rtl_cu_rx_held(d);
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

static bool rtl_cu_sample_rf0(struct xhci_device *d,enum rtl_cu_rf0_phase phase) {
    if(!rtl_cu_rf_read(d,0,&rtl_passive.rf0[phase]))return false;
    rtl_passive.rf0_valid|=1u<<phase;
    return true;
}

static bool rtl_cu_enable_rf_path_mux(struct xhci_device *d) {
    /* The 92C Linux single-chain path setup asserts LEDCFG bit 23. */
    if(!rtl8188cu_read(d,RTL_CU_LED_CONFIG,4,&rtl_passive.ledcfg_before))return false;
    rtl_passive.ledcfg_valid=1;
    uint32_t expected=rtl_passive.ledcfg_before|RTL_CU_RF_PATH_MUX;
    if((expected!=rtl_passive.ledcfg_before && !rtl8188cu_write_value(d,RTL_CU_LED_CONFIG,4,expected)) ||
       !rtl8188cu_read(d,RTL_CU_LED_CONFIG,4,&rtl_passive.ledcfg_after))return false;
    rtl_passive.ledcfg_valid=3;
    return rtl_passive.ledcfg_after==expected;
}

static bool rtl_cu_apply_linux_radio_settings(struct xhci_device *d) {
    if(!rtl_cu_tx_parked(d) || !rtl_cu_sample_rf0(d,RTL_CU_RF0_BEFORE_WRITE) ||
       !rtl_cu_enable_rf_path_mux(d) ||
       !rtl8188cu_update(d,RTL_CU_RF_PATH_SELECT,4,0,1u<<13) ||
       !rtl8188cu_write_value(d,0x840,4,RTL_CU_RF0_LINUX_WRITE))return false;
    rtl_passive.rf0_write_ok=true;
    return rtl_cu_sample_rf0(d,RTL_CU_RF0_PRE_LC);
}

static bool rtl_cu_capture_phy_radio(struct xhci_device *d) {
    if(!rtl_cu_sample_rf0(d,RTL_CU_RF0_TUNED))return false;
    uint32_t path=0;
    for(unsigned i=0;i<RTL_CU_PHY_RADIO_COUNT;i++) {
        uint16_t address=rtl_cu_phy_radio_regs[i].address;
        if(!rtl8188cu_read(d,address,rtl_cu_phy_radio_regs[i].width,&rtl_passive.phy_radio[i]))return false;
        rtl_passive.phy_radio_reads++;
        if(address==RTL_CU_RF_PATH_SELECT)path=rtl_passive.phy_radio[i];
    }
    rtl_passive.phy_radio_ready=path==0x2808 &&
        (rtl_passive.rf0[RTL_CU_RF0_TUNED]&RTL_CU_RF0_STABLE_MASK)==RTL_CU_RF0_STABLE_RX;
    return rtl_passive.phy_radio_ready;
}

static bool rtl_cu_apply_iq_matrix(struct xhci_device *d) {
    /* Retained path-A matrix from the working Linux capture on this board.
     * TX stays parked and none of the IQK trigger registers are written. */
    static const struct { uint16_t address; uint32_t mask,value; } fields[RTL_CU_IQ_FIELDS]={
        [RTL_CU_IQ_RX]={RTL_CU_RX_IQ,0x0000ffff,0x04f7},
        [RTL_CU_IQ_RX_EXT]={RTL_CU_RX_IQ_EXT,0xf0000000,0},
        [RTL_CU_IQ_TX]={0xc80,0x003f03ff,0x000b0102},
        [RTL_CU_IQ_TX_EXT]={0xc94,0xf0000000,0},
        [RTL_CU_IQ_TX_CCA]={0xc4c,0xa0000000,0},
    };
    rtl_passive.iq_attempted=true;
    uint32_t mac,sifs_before=0,iq_mode=0,e4c_before=0;
    if(!rtl_cu_tx_parked(d) || !rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&mac) ||
       mac!=RTL_CU_RX_ENABLE)return false;
    for(unsigned i=0;i<RTL_CU_IQ_STARTUP_COUNT;i++) {
        uint16_t address=rtl_cu_iq_startup_regs[i];
        if(!rtl8188cu_read(d,address,4,&rtl_passive.iq_startup[i]))return false;
        rtl_passive.iq_startup_reads++;
        if(address==RTL_CU_LED_CONFIG && !(rtl_passive.iq_startup[i]&RTL_CU_RF_PATH_MUX))return false;
        if(address==RTL_CU_SPEC_SIFS)sifs_before=rtl_passive.iq_startup[i];
        if(address==RTL_CU_IQ_MODE)iq_mode=rtl_passive.iq_startup[i];
        if(address==RTL_CU_IQ_CAL_CONTROL)e4c_before=rtl_passive.iq_startup[i];
    }
    if(iq_mode)return false;
    /* SPEC_SIFS is a halfword. The upper half of this read is REG_RL. */
    rtl_passive.sifs_valid=rtl8188cu_write_value(d,RTL_CU_SPEC_SIFS,2,RTL_CU_SPEC_SIFS_LINUX) &&
        rtl8188cu_read(d,RTL_CU_SPEC_SIFS,4,&rtl_passive.sifs_readback);
    if(!rtl_passive.sifs_valid ||
       rtl_passive.sifs_readback!=((sifs_before&0xffff0000u)|RTL_CU_SPEC_SIFS_LINUX))return false;
    /* Match the full word retained by the Linux monitor on this board. */
    rtl_passive.b68_attempted=true;
    rtl_passive.b68_valid=rtl8188cu_write_value(d,RTL_CU_IQ_STARTUP_B68,4,RTL_CU_IQ_STARTUP_B68_LINUX) &&
        rtl8188cu_read(d,RTL_CU_IQ_STARTUP_B68,4,&rtl_passive.b68_readback);
    if(!rtl_passive.b68_valid || rtl_passive.b68_readback!=RTL_CU_IQ_STARTUP_B68_LINUX)return false;
    /* Match the retained calibration control with IQ mode clear. */
    uint32_t e4c_expected=e4c_before|RTL_CU_IQ_CAL_CONTROL_LINUX;
    rtl_passive.e4c_valid=(e4c_expected==e4c_before || rtl8188cu_write_value(d,RTL_CU_IQ_CAL_CONTROL,4,e4c_expected)) &&
        rtl8188cu_read(d,RTL_CU_IQ_CAL_CONTROL,4,&rtl_passive.e4c_readback);
    if(!rtl_passive.e4c_valid || rtl_passive.e4c_readback!=e4c_expected)return false;
    for(unsigned i=0;i<RTL_CU_IQ_FIELDS;i++) {
        uint32_t before;
        if(!rtl8188cu_read(d,fields[i].address,4,&before))return false;
        rtl_passive.iq_before[i]=before;
        rtl_passive.iq_valid|=1u<<(2*i);
        uint32_t desired=(before&~fields[i].mask)|fields[i].value;
        if((before!=desired && !rtl8188cu_write_value(d,fields[i].address,4,desired)) ||
           !rtl8188cu_read(d,fields[i].address,4,&rtl_passive.iq_after[i]))return false;
        rtl_passive.iq_valid|=1u<<(2*i+1);
        if(rtl_passive.iq_after[i]!=desired)return false;
    }
    rtl_passive.iq_ready=true;
    return true;
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
       !rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&mac) || mac!=RTL_CU_RX_ENABLE ||
       !rtl8188cu_read(d,0xd03,1,&test_mode) || (test_mode&0x70) ||
       !rtl8188cu_write_value(d,0x840,4,0x01800000u|RTL_CU_RF_START_CHANNEL|RTL_CU_LC_START))return false;
    rtl_passive.post_lc_triggered=true;
    uint64_t before=timer_monotonic_us();
    if(!timer_delay_us(100000))return false;
    rtl_passive.post_lc_wait_us=timer_monotonic_us()-before;
    if(!rtl8188cu_check_port(d,hub) || !rtl_cu_tx_parked(d) ||
       !rtl_cu_sample_rf0(d,RTL_CU_RF0_POST_LC) ||
       !rtl_cu_rf_read(d,0x18,&rtl_passive.post_lc_channel))return false;
    rtl_passive.post_lc_read_valid=true;
    return (rtl_passive.post_lc_channel&RTL_CU_RF_DATA_MASK)==RTL_CU_RF_START_CHANNEL;
}

static bool rtl_cu_prepare_rx_dma(struct xhci_device *d) {
    rtl_passive.dma_attempted=true;
    if(!rtl8188cu_read(d,RTL_CU_TRXDMA_CONTROL,2,&rtl_passive.trxdma_before))return false;
    rtl_passive.dma_before_valid=1;
    if(!rtl8188cu_read(d,RTL_CU_USB_OPTION,1,&rtl_passive.usb_opt_before))return false;
    rtl_passive.dma_before_valid=3;

    /* Linux's receive bits are clear. The remaining TRXDMA bits include
     * TX queue routing, so only edit the affected low byte. */
    uint32_t trxdma=rtl_passive.trxdma_before&~RTL_CU_TRXDMA_RX_BITS;
    uint32_t usbopt=rtl_passive.usb_opt_before&~RTL_CU_USB_RX_AGG;
    if((trxdma!=rtl_passive.trxdma_before && !rtl8188cu_write_value(d,RTL_CU_TRXDMA_CONTROL,1,trxdma&0xff)) ||
       (usbopt!=rtl_passive.usb_opt_before && !rtl8188cu_write_value(d,RTL_CU_USB_OPTION,1,usbopt)))return false;
    for(unsigned i=0;i<RTL_CU_DMA_COUNT;i++) {
        if(!rtl8188cu_read(d,rtl_cu_dma_regs[i].address,rtl_cu_dma_regs[i].width,&rtl_passive.dma_regs[i]))return false;
        rtl_passive.dma_reads++;
    }
    rtl_passive.dma_ready=rtl_passive.dma_regs[RTL_CU_DMA_TRXDMA]==trxdma &&
        rtl_passive.dma_regs[RTL_CU_DMA_USB_OPTION]==usbopt;
    return rtl_passive.dma_ready;
}

static bool rtl_cu_finish_mac_power(struct xhci_device *d) {
    /* The CU Linux power-on routine applies this state after MAC auto-on
     * completes and before clearing RF digital isolation. */
    rtl_passive.aps_attempted=true;
    if(!rtl8188cu_read(d,RTL8188CU_POWER,2,&rtl_passive.aps_before))return false;
    rtl_passive.aps_reads=1;
    if((rtl_passive.aps_before&(RTL8188CU_AUTOLOAD|RTL8188CU_MAC_ON))!=RTL8188CU_AUTOLOAD ||
       (rtl_passive.aps_before!=RTL_CU_APS_RADIO_GPIO_LED &&
        !rtl8188cu_write_value(d,RTL8188CU_POWER,2,RTL_CU_APS_RADIO_GPIO_LED)) ||
       !rtl8188cu_read(d,RTL8188CU_POWER,2,&rtl_passive.aps_after))return false;
    rtl_passive.aps_reads=2;
    rtl_passive.aps_ready=rtl_passive.aps_after==RTL_CU_APS_RADIO_GPIO_LED;
    return rtl_passive.aps_ready;
}

static bool rtl_cu_prepare_xtal(struct xhci_device *d) {
    /* The Linux 1T PHY table places this dword ahead of the 0x800 rows. */
    rtl_passive.xtal_attempted=true;
    if(!rtl8188cu_read(d,RTL_CU_AFE_XTAL_CONTROL,4,&rtl_passive.xtal_before))return false;
    rtl_passive.xtal_reads=1;
    if(!rtl8188cu_write_value(d,RTL_CU_AFE_XTAL_CONTROL,4,RTL_CU_AFE_XTAL_LINUX) ||
       !rtl8188cu_read(d,RTL_CU_AFE_XTAL_CONTROL,4,&rtl_passive.xtal_after))return false;
    rtl_passive.xtal_reads=2;
    rtl_passive.xtal_ready=rtl_passive.xtal_after==RTL_CU_AFE_XTAL_LINUX;
    return rtl_passive.xtal_ready;
}

static const char *rtl_cu_prepare_passive_start(struct xhci_device *d,struct xhci_device *hub) {
    uint32_t state;
    if(rtl_passive.owner || !rtl8188cu_check_port(d,hub))return "passive-admission";
    rtl_passive.owner=d;
    if(!rtl8188cu_write_value(d,RTL_CU_TX_PAUSE,1,0xff) || !rtl_cu_tx_parked(d) ||
       !rtl8188cu_power_sample(d,RTL8188CU_POWER_BEFORE_APS) ||
       !rtl_cu_finish_mac_power(d) ||
       !rtl8188cu_power_sample(d,RTL8188CU_POWER_BEFORE_RF_RELEASE) ||
       !rtl8188cu_update(d,RTL8188CU_ISOLATION,2,1u<<9,0) ||
       !rtl8188cu_power_sample(d,RTL8188CU_POWER_AFTER_RF_RELEASE) ||
       !rtl8188cu_update(d,0x600,1,1u<<6,0) ||
       !rtl8188cu_poll(d,0x600,1,1u<<7,0,32,&state))return "mac-power";
    if(!rtl_cu_open_rx_early(d))return "early-rx-gate";
    rtl_passive.buffer_ready=rtl_cu_packet_buffer(d,hub);
    return rtl_passive.buffer_ready?NULL:"packet-buffer";
}

static bool rtl_cu_radio_prepare(struct xhci_device *d,struct xhci_device *hub,
                                 const uint8_t *blob,unsigned *writes,uint32_t *channel) {
    if(!rtl_cu_rx_held(d) ||
       !rtl8188cu_update(d,RTL8188CU_SYS_FUNC,2,0,0x2003) ||
       !rtl8188cu_write_value(d,0x28,1,0x83) || !rtl8188cu_write_value(d,0x29,1,0xdb) ||
       !rtl8188cu_write_value(d,RTL8188CU_RF_CONTROL,1,7) ||
       !rtl8188cu_write_value(d,RTL8188CU_SYS_FUNC,1,0x17) ||
       !rtl8188cu_update(d,0x87c,4,1u<<31,0) ||
       !rtl8188cu_write_value(d,0x22,1,0x0f) ||
       !rtl8188cu_write_value(d,0x25,1,0x80) || !rtl_cu_prepare_xtal(d))return false;
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
       !rtl_cu_apply_linux_radio_settings(d) ||
       !rtl_cu_post_profile_lc(d,hub))return false;
    /* Verify the channel through RF path A and keep both aggregation paths
     * off so a single receive buffer describes one frame. */
    if(!rtl8188cu_update(d,0x603,1,0,4) ||
       !rtl8188cu_update(d,0x800,4,1,0) ||
       !rtl8188cu_update(d,0x900,4,1,0) ||
       !rtl8188cu_write_value(d,0x840,4,0x0180740a) ||
       !rtl_cu_rf_read(d,0x18,channel) || (*channel&0x3ff)!=RTL_CU_CHANNEL ||
       !rtl_cu_prepare_rx_dma(d) ||
       !rtl8188cu_write_value(d,0x60f,1,RTL_CU_RX_DRIVER_INFO_UNITS) ||
       !rtl8188cu_write_value(d,0x608,4,RTL_CU_MONITOR_RCR|RTL_CU_RCR_PHY_STATUS) ||
       !rtl8188cu_write_value(d,0x620,4,UINT32_MAX) || !rtl8188cu_write_value(d,0x624,4,UINT32_MAX) ||
       !rtl8188cu_write_value(d,0x6a0,2,RTL_CU_ALL_SUBTYPES) ||
       !rtl8188cu_write_value(d,0x6a2,2,0) ||
       !rtl8188cu_write_value(d,0x6a4,2,RTL_CU_ALL_SUBTYPES))return false;
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
    if(!rtl_cu_tx_parked(d) ||
       !rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&rtl_passive.rx_cr))return false;
    rtl_passive.rx_cr_valid=true;
    if(rtl_passive.rx_cr!=RTL_CU_RX_ENABLE)return false;
    for(unsigned i=0;i<RTL_CU_ENABLE_COUNT;i++) {
        if(!rtl8188cu_read(d,rtl_cu_enable_regs[i].address,rtl_cu_enable_regs[i].width,
                          &rtl_passive.enable_regs[i]))return false;
        rtl_passive.enable_reads++;
    }
    if(rtl_passive.enable_regs[RTL_CU_ENABLE_XTAL]!=RTL_CU_AFE_XTAL_LINUX)return false;
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

static void rtl_cu_park(struct xhci_device *d,struct xhci_device *hub) {
    if(rtl_passive.owner!=d || !hub || d->control_failed || d->ep0_quarantined ||
       d->control_pending || !rtl8188cu_check_port(d,hub))return;
    uint32_t mac,rf;
    bool rx_off=rtl8188cu_update(d,RTL8188CU_MAC_CONTROL,1,RTL_CU_RX_ENABLE,0);
    bool rf_off=rtl8188cu_update(d,RTL8188CU_RF_CONTROL,1,RTL8188CU_RF_ENABLE,0);
    rtl_passive.parked=rx_off && rf_off && rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,1,&mac) &&
        rtl8188cu_read(d,RTL8188CU_RF_CONTROL,1,&rf) &&
        !(mac&(RTL_CU_RX_ENABLE|RTL_CU_TX_GATES)) && !(rf&RTL8188CU_RF_ENABLE);
}

static bool rtl_cu_capture_passive_window(const uint8_t *blob,bool ready) {
    const char *result="admission";
    unsigned writes=0;
    uint32_t channel=0;
    bool stopped=false,window=false;
    uint64_t flags=event_irq_save();
    struct xhci_device *d=rtl_passive.owner;
    if(d && !ready)result="startup";
    if(d && ready) {
        struct xhci_device *hub=device_by_slot(d->parent_slot_id);
        if(hub && !xhci.sync_users && !strict_control.device) {
            d->rtl8188cu_firmware_active=true;
            result="setup";
            if(wifi_hub_path_present(d) && timer_reference_ready() && rtl8188cu_check_port(d,hub) &&
               rtl_cu_radio_prepare(d,hub,blob,&writes,&channel)) {
                bool rx_setup=rtl_cu_verify_receive_setup(d);
                bool phy_radio=rx_setup && rtl_cu_capture_phy_radio(d);
                result=phy_radio?"rx-endpoint":rx_setup?"phy-radio":"rx-setup";
                if(phy_radio && !rtl_cu_apply_iq_matrix(d)) {
                    result="iq-matrix";
                    phy_radio=false;
                }
                if(phy_radio && rtl_cu_rx_configure(d)) {
                    window=rtl_cu_listen(d,hub);
                    rtl_cu_rx_snapshot(d,RTL_CU_BEFORE_STOP,window);
                    window=window && !rtl_passive.failed && !d->control_failed && !d->ep0_quarantined;
                    stopped=rtl_cu_rx_stop(d);
                    rtl_cu_rx_snapshot(d,RTL_CU_AFTER_STOP,false);
                    result=window && !rtl_passive.completed?"rx-timeout":"window";
                    if(rtl_passive.enable_reads==RTL_CU_ENABLE_COUNT &&
                       rtl_passive.enable_regs[RTL_CU_ENABLE_XTAL]!=RTL_CU_AFE_XTAL_LINUX)result="xtal-drift";
                }
            } else if(rtl_passive.xtal_attempted && !rtl_passive.xtal_ready) {
                result="xtal";
            }
            rtl_cu_park(d,hub);
            d->rtl8188cu_firmware_active=false;
        }
        if(window && stopped && rtl_passive.parked && rtl_passive.completed)
            result=rtl_passive.matches>=2?"target":"target-absent";
    }
    event_irq_restore(flags);
    rtl_cu_rx_log();
    if(rtl_passive.owner)
        console_printf("rtl8188cu-packet-buffer: ready=%u linked=%u first_completion_us=%u\n",
            (uint64_t)rtl_passive.buffer_ready,(uint64_t)rtl_passive.linked,rtl_passive.first_completion_us);
    console_printf("rtl8188cu-passive: result=%s writes=%u rf_channel=%x posted=%u completed=%u beacons=%u target=%u rejected=%u stopped=%u parked=%u\n",
        result,(uint64_t)writes,(uint64_t)channel,(uint64_t)rtl_passive.posted,(uint64_t)rtl_passive.completed,
        (uint64_t)rtl_passive.beacons,(uint64_t)rtl_passive.matches,(uint64_t)rtl_passive.rejected,
        (uint64_t)stopped,(uint64_t)rtl_passive.parked);
    return window && stopped && rtl_passive.parked && rtl_passive.matches>=2;
}

bool xhci_rtl8188cu_capture_passive(const uint8_t *image,uint64_t image_size,
                                    const uint8_t *profile,uint64_t profile_size) {
    if(rtl_passive.owner)return false;
    bool ready=rtl_cu_profile_valid(profile,profile_size) && rtl8188cu_capture_firmware(image,image_size,true);
    return rtl_cu_capture_passive_window(profile,ready);
}
