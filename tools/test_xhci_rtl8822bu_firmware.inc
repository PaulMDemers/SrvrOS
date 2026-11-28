enum bu_firmware_fault {
    BU_FW_OK,BU_FW_FLASH,BU_FW_COLD_LATCH,BU_FW_COLD_DMA_BUSY,BU_FW_OTHER_LATCH,BU_FW_OTHER_STATUS,
    BU_FW_IMAGE,BU_FW_WARM,BU_FW_ENTRY_MCU,BU_FW_PLATFORM_MCU,
    BU_FW_SHORT,BU_FW_MISSING,BU_FW_CHECKSUM,BU_FW_NOT_READY,BU_FW_BAD_STOP,BU_FW_BUSY,
    BU_BOARD_OK,BU_BOARD_BAD_BANK,BU_BOARD_BAD_MODE,BU_BOARD_WRONG_ADDR,
    BU_BOARD_WRONG_RFE,BU_BOARD_NO_REPLY,BU_BOARD_WRONG_FEATURE,BU_BOARD_NO_ACK,BU_BOARD_BAD_STOP,
    BU_PHY_OK,BU_PHY_BAD_PROFILE,BU_PHY_BAD_CRYSTAL,BU_PHY_STUCK_RF,
    BU_RX_OK,BU_RX_TIMEOUT,BU_RX_WRONG_EVENT,BU_RX_BAD_STOP,BU_RX_TUNE_FAIL,BU_RX_CLOSING,
    BU_RX_LLT_STUCK,BU_RX_FILTER_DRIFT,
    BU_RX_DEPTH_OK,BU_RX_DEPTH_TIMEOUT,BU_RX_DEPTH_TRANSITION,BU_RX_EARLY_VALID,BU_RX_SEC_RESTORE_DRIFT,BU_RX_SEC_SETUP_DRIFT,
    BU_RX_FIFO_BOUNDARY_DRIFT,BU_RX_FIFO_RESTORE_DRIFT,BU_RX_EP_AVG_DRIFT,BU_RX_DEPTH_DUPLICATE,BU_RX_EARLY_STALE,BU_RX_EARLY_FW_SHORT,
    BU_RX_PAUSE_RESET,BU_RX_PAUSE_REFUSED,
    BU_RX_MAC_SELECTOR_DRIFT,BU_RX_MAC_RESET_BUSY,BU_RX_MAC_RESTORE_DRIFT,
    BU_H2C_BAD_LIMIT,BU_H2C_INIT_STUCK,BU_H2C_BAD_ENTRY,BU_H2C_OUT_SHORT,BU_H2C_OUT_MISSING,
    BU_H2C_OUT_WRONG_TRB,BU_H2C_UNDRAINED,BU_H2C_RESTORE_DRIFT,
};

static struct {
    const uint8_t *image,*phy;
    uint8_t reg[65536];
    uint64_t ring,cursor,packet,rx_ring,rx_cursor;
    uint32_t written;
    unsigned posted,ddma,configure,stop,echoes,register_writes,latch_clears,efuse_reads,feature_acks;
    uint8_t echo;
    bool echo_expected,phy_started;
    unsigned phy_writes,phy_delays,pause_writes;
    unsigned rx_posts,rx_stops,channel_writes,rx_samples,rx_page_writes,rx_llt_polls,h2c_setup,h2c_posts;
    unsigned out_state_logs,out_event_logs,out_device_logs;
    unsigned h2c_status_writes,h2c_status_reads;
    unsigned rx_security_policy_writes,rx_fifo_writes;
    unsigned rx_mac_writes,rx_mac_reads;
    bool rx_security_rcr_reloaded,rx_security_cr_restored,rx_dma_written;
    bool rx_queue_mapped,rx_llt_requested,rx_filters_written,rx_security_written,rx_depth_event;
    enum bu_firmware_fault fault;
} bu_model;

static uint32_t bu_model_read(unsigned reg,unsigned width) {
    uint32_t value=0;
    for(unsigned i=0;i<width;i++)value|=(uint32_t)bu_model.reg[reg+i]<<(8*i);
    return value;
}
static bool bu_model_cold_board(void) {
    return bu_model.fault==BU_FW_COLD_LATCH || bu_model.fault>=BU_BOARD_OK;
}
static void bu_model_store(unsigned reg,unsigned width,uint32_t value) {
    for(unsigned i=0;i<width;i++)bu_model.reg[reg+i]=(uint8_t)(value>>(8*i));
}

static void bu_model_command(struct xhci_device *d) {
    unsigned idx=(xhci.command_enqueue+XHCI_TRB_COUNT-2)%(XHCI_TRB_COUNT-1);
    const struct xhci_trb *command=&xhci.command_ring[idx];
    unsigned type=(command->control>>10)&63;
    assert(command->control>>24==4);
    if(type==XHCI_TRB_TYPE_CONFIGURE_ENDPOINT) {
        unsigned dci=bu_model.configure++?BU_RX_DCI:BU_OUT_DCI;
        assert(context_read32(d->input_context,0,1)==(1u|(1u<<dci)));
        assert(context_read32(d->input_context,1,0)>>27==10);
        assert(context_read32(d->input_context,dci+1,1)==(6u|((dci==BU_RX_DCI?6u:2u)<<3)|(512u<<16)));
        uint64_t ring=context_read32(d->input_context,dci+1,2) |
                      (uint64_t)context_read32(d->input_context,dci+1,3)<<32;
        assert((ring&4095)==1);
        if(dci==BU_RX_DCI) {
            assert(bu_model.configure==2 && !bu_model.channel_writes && !bu_model_read(BU_CR,1));
            assert(bu_model.cursor==bu_model.ring && bu_out_dequeue(d)==bu_model.ring &&
                   !bu_model.ddma && bu_model_read(BU_TXPAUSE,1)==0xff);
            assert(context_read32(d->input_context,dci+1,4)==0);
            bu_model.rx_ring=bu_model.rx_cursor=ring;
            context_write32(d->output_context,dci,1,0x02000036);
            context_write32(d->output_context,dci,4,bu_model.fault==BU_RX_EP_AVG_DRIFT?32768:0);
            context_write32(d->output_context,dci,0,1);
            context_write32(d->output_context,dci,2,(uint32_t)ring);
            context_write32(d->output_context,dci,3,ring>>32);
        } else {
            bu_model.ring=bu_model.cursor=ring;
            context_write32(d->output_context,0,0,10u<<27);
            context_write32(d->output_context,dci,0,1);
            context_write32(d->output_context,dci,1,0x02000016);
            context_write32(d->output_context,dci,2,(uint32_t)ring);
            context_write32(d->output_context,dci,3,ring>>32);
        }
    } else {
        unsigned dci=(command->control>>16)&31;
        uint64_t cursor;
        assert(type==XHCI_TRB_TYPE_STOP_ENDPOINT);
        if(dci==BU_RX_DCI) {
            assert(!bu_model.rx_stops++ && !bu_model_read(BU_CR,1));
            cursor=bu_model.rx_cursor+(bu_model.fault==BU_RX_BAD_STOP?80:0);
        } else {
            assert(dci==BU_OUT_DCI && !bu_model.stop++);
            cursor=bu_model.cursor;
            if(bu_model.fault==BU_FW_BAD_STOP || bu_model.fault==BU_BOARD_BAD_STOP)cursor+=32;
        }
        context_write32(d->output_context,dci,0,3);
        context_write32(d->output_context,dci,2,(uint32_t)cursor);
        context_write32(d->output_context,dci,3,cursor>>32);
    }
    uint64_t pointer=xhci.command_ring_phys+idx*16;
    xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)pointer,
        .parameter_high=pointer>>32,.status=1u<<24,.control=(33u<<10)|(4u<<24)|xhci.event_cycle};
}

static unsigned bu_model_block_size(unsigned index) {
    unsigned end=index<3?11216:161176;
    return end-bu_model.written>4096?4096:end-bu_model.written;
}

static void bu_model_receive_event(bool closing,unsigned ahead);

static void bu_model_bulk(struct xhci_device *d) {
    bool passive=bu_model.fault>=BU_RX_OK;
    assert(bu_model.configure==(passive?2u:1u) && !bu_model.stop && bu_model.posted==bu_model.ddma);
    if(passive)assert(bu_rx_owner && bu_model.rx_posts==4 && bu_model_read(BU_TXPAUSE,1)==0xff);
    assert(bu_model_read(0x100,2)==0x105 && !(bu_model_read(0x550,1)&8) &&
           (bu_model_read(0x80,1)&1) && !(bu_model_read(3,1)&4) && !(bu_model_read(0x1d,1)&1));
    assert(bu_model_read(0xec,4)==0x18000004u);
    if(bu_model.fault==BU_FW_FLASH)assert(bu_model_read(0x40,4)==0x0204 && !(bu_model_read(0x80,4)&(1u<<20)));
    assert(d->bulk_pending && d->bulk_dci==10 && d->bulk_trb==(bu_model.cursor&~1ull));
    const struct xhci_trb *trb=(const void *)(uintptr_t)d->bulk_trb;
    unsigned count=bu_model_block_size(bu_model.posted),padded=count+((count+48)%512==0);
    bu_model.packet=trb->parameter_low|(uint64_t)trb->parameter_high<<32;
    assert((bu_model.packet&0xffff)+trb->status<=0x10000 && trb->status==padded+48);
    const uint8_t *packet=(const void *)(uintptr_t)bu_model.packet;
    uint8_t header[48]={padded,padded>>8,0x30,0x04,0,0x10};
    uint16_t checksum=(uint16_t)padded^0x0430u^0x1000u;
    header[28]=checksum;header[29]=checksum>>8;
    assert(!memcmp(packet,header,sizeof header));
    assert(!memcmp(packet+48,bu_model.image+64+bu_model.written,count));
    if(padded>count)assert(!packet[48+count]);
    bu_model.posted++;
    if(bu_model.fault==BU_FW_MISSING)return;
    bu_model.cursor+=16;
    unsigned cc=bu_model.fault==BU_FW_SHORT || bu_model.fault==BU_RX_EARLY_FW_SHORT?13:1;
    bool early=bu_model.posted==1 &&
        (bu_model.fault==BU_RX_EARLY_VALID || bu_model.fault==BU_RX_EARLY_STALE);
    if(early)bu_model_receive_event(true,0);
    unsigned index=xhci.event_dequeue+early;
    bool cycle=xhci.event_cycle;
    if(index==XHCI_EVENT_TRB_COUNT){index=0;cycle=!cycle;}
    xhci.event_ring[index]=(struct xhci_trb){
        .parameter_low=(uint32_t)d->bulk_trb,.parameter_high=d->bulk_trb>>32,
        .status=(cc<<24)|(cc==13?1:0),.control=(32u<<10)|(4u<<24)|(10u<<16)|cycle};
    bu_model_store(0x204,2,0x8000);
}

static void bu_model_h2c(struct xhci_device *d) {
    /* Expected framing from the Linux USB capture. */
    static const uint8_t expected[2][80]={
        {[0]=0x20,[5]=0x13,[28]=0x20,[29]=0x13,[48]=1,[49]=0xff,[50]=0x0d,[52]=0x0c,[58]=0x30},
        {[0]=0x20,[5]=0x13,[28]=0x20,[29]=0x13,[48]=1,[49]=0xff,[50]=0x11,[52]=0x10,
         [54]=1,[56]=3,[57]=2,[58]=3,[59]=0x33},
    };
    unsigned seq=bu_model.h2c_posts++;
    assert(seq<2 && bu_model.rx_posts==4 && bu_model.ddma==40 && bu_model.configure==2 && bu_model.h2c_setup==6);
    assert(bu_model_read(BU_CR,1)==0x9f && bu_model_read(BU_TXPAUSE,1)==0xff && d->bulk_pending);
    assert(bu_model.h2c_status_writes==1 && bu_model.h2c_status_reads==2 &&
           bu_model_read(BU_H2C_INFO,1)==0x09 && bu_model_read(BU_H2C_TAIL,4)==0x3fe00);
    assert(d->bulk_trb==(bu_model.cursor&~1ull) && bu_out_dequeue(d)==bu_model.ring);
    const struct xhci_trb *trb=(const void *)(uintptr_t)d->bulk_trb;
    uint64_t phys=trb->parameter_low|(uint64_t)trb->parameter_high<<32;
    assert(trb->status==80 && trb->control==0x421 && (phys&0xffff)+80<=0x10000);
    assert(!memcmp((const void *)(uintptr_t)phys,expected[seq],sizeof expected[seq]));
    if(bu_model.fault==BU_H2C_OUT_MISSING) {
        test_time_step=10000;
        return;
    }
    uint64_t ptr=d->bulk_trb;
    if(bu_model.fault==BU_H2C_OUT_WRONG_TRB) {
        ptr-=16;
        test_time_step=10000;
    } else bu_model.cursor+=16;
    unsigned cc=bu_model.fault==BU_H2C_OUT_SHORT?13:1;
    xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){
        .parameter_low=(uint32_t)ptr,.parameter_high=ptr>>32,
        .status=(cc<<24)|(cc==13?1:0),.control=(32u<<10)|(4u<<24)|(10u<<16)|xhci.event_cycle};
    if(cc==1 && ptr==d->bulk_trb) {
        bu_model_store(BU_H2C_WP,4,BU_H2C_START+32*(seq+1));
        if(bu_model.fault!=BU_H2C_UNDRAINED)bu_model_store(BU_H2C_RP,4,BU_H2C_START+32*(seq+1));
    }
}

static void bu_model_out_log(const char *fmt) {
    if(!strncmp(fmt,"rtl8822bu-out-state:",20))bu_model.out_state_logs++;
    if(!strncmp(fmt,"rtl8822bu-out-event:",20))bu_model.out_event_logs++;
    if(!strncmp(fmt,"rtl8822bu-h2c-out-device:",25))bu_model.out_device_logs++;
}

static void bu_model_receive_completion(bool closing);

static void bu_model_control(void) {
    struct xhci_device *d=&xhci.devices[3];
    assert(strict_control.device==d);
    const struct xhci_trb *setup=(const void *)(uintptr_t)strict_control.setup;
    unsigned reg=setup->parameter_low>>16,width=setup->parameter_high>>16;
    bool in=strict_control.data_in;
    bool closing=false;
    assert((setup->parameter_low&0xffff)==(in?0x05c0:0x0540) && !(setup->parameter_high&0xffff));
    assert((width==1 || width==2 || width==4) && reg+width<=sizeof bu_model.reg);
    if(bu_model.echo_expected) {
        assert(!in && reg==0x4e0 && width==1 && d->descriptor_buffer[0]==bu_model.echo);
        bu_model.echo_expected=false;bu_model.echoes++;
    } else if(in) {
        unsigned cr=bu_model_read(BU_CR,1);
        if(reg==BU_H2C_STATUS) {
            assert(cr==0x9f && width==4 && bu_model_read(BU_TXPAUSE,1)==0xff);
            bu_model.h2c_status_reads++;
            bu_model_store(reg,width,0xeaeaeaea);
        }
        if(bu_rx_owner && cr==0xbf && (reg==0xfcc || bu_model.rx_samples%10)) {
            static const unsigned registers[]={0xfcc,0xf08,0xf04,0xf14,0xf10,0xf0c,0x284,0x288,0x28c,0x2b0};
            unsigned i=bu_model.rx_samples++;
            assert(i<20 && reg==registers[i%10] && width==4 &&
                bu_model_read(BU_CR,2)==0x06bf && bu_model_read(BU_RX_SECCFG,2)==0x31cf &&
                bu_model_read(BU_TXPAUSE,1)==0xff && bu_model_read(0x11c,4)==0x5eff &&
                bu_model_read(0x10c,2)==0xf5a1 && bu_model_read(0x280,4)==0x2003);
            assert(bu_model.rx_posts>=4);
            uint32_t observed=i%20>=10?0x000b0005:0x00020003;
            if(bu_model.fault>=BU_RX_DEPTH_OK && i%10>=6)observed=reg==0x284?0x20000:0;
            bu_model_store(reg,width,observed);
            closing=(i==10 && bu_model.fault==BU_RX_CLOSING) || (i==0 && bu_model.fault==BU_RX_DEPTH_TRANSITION);
        }
        if(bu_model.rx_llt_requested && reg==BU_RX_LLT && width==4)bu_model.rx_llt_polls++;
        if(reg==0x11c && cr==0xbf && bu_model.fault==BU_RX_FIFO_BOUNDARY_DRIFT)
            bu_model_store(reg,width,0x6000);
        if(reg==BU_RX_MAC_REPORT) {
            unsigned count=bu_model.rx_mac_reads++;
            assert(count<8 && width==4 && cr==0xbf && bu_model_read(BU_TXPAUSE,1)==0xff);
            if(bu_model.fault==BU_RX_MAC_SELECTOR_DRIFT)
                assert(bu_model.rx_mac_writes==2*count+1);
            else
                assert(bu_model.rx_mac_writes==5*(count/4)+count%4+1);
            bu_model_store(reg,2,0x1200+count);
        }
        memcpy(d->descriptor_buffer,bu_model.reg+reg,width);
        if(reg==BU_RX_MAC_REPORT && bu_model.fault==BU_RX_MAC_SELECTOR_DRIFT)
            d->descriptor_buffer[3]^=0x10;
    } else {
        bu_model.register_writes++;
        uint32_t value=0;
        for(unsigned i=0;i<width;i++)value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
        if(reg==BU_TXPAUSE) {
            assert(bu_model.fault>=BU_PHY_OK && value==0xff && !bu_model_read(BU_CR,1));
            if(!bu_model.pause_writes)
                assert(bu_model.feature_acks==(bu_model.fault>=BU_RX_OK?0u:1u));
            else
                assert(bu_model.fault==BU_RX_PAUSE_RESET || bu_model.fault==BU_RX_PAUSE_REFUSED);
            bu_model.pause_writes++;
            if(bu_model.fault==BU_RX_PAUSE_REFUSED && bu_model.pause_writes>1)value=0;
            bu_model.phy_started=true;
        }
        if(reg==BU_RX_MAC_REPORT+3) {
            static const uint8_t expected[]={0x32,0x02,0x42,0x12,0x56};
            unsigned count=bu_model.rx_mac_writes++;
            assert(count<10 && width==1 && !(value&8) && bu_model_read(BU_CR,1)==0xbf &&
                   bu_model_read(BU_TXPAUSE,1)==0xff && bu_model.rx_posts>=4);
            unsigned step=count%5;
            if(bu_model.fault==BU_RX_MAC_SELECTOR_DRIFT)step=count%2?4:0;
            assert(value==expected[step]);
            if(bu_model.fault==BU_RX_MAC_RESTORE_DRIFT && step==4)value^=0x10;
        }
        if(reg==2 && (value&3))assert(bu_model.phy_started && (value&3)==3);
        if(reg==0x1f && (value&7))assert(bu_model.phy_started && (value&7)==7);
        if(reg==0xec)assert(value==0x18000004u || (bu_model.phy_started && value==0x1f000004u));
        if(bu_model.phy_started && bu_model.phy_writes<BU_PHY_ROWS && (bu_model.phy_writes || reg==0x29)) {
            const uint8_t *row=bu_model.phy+BU_PHY_ROW_BYTES*bu_model.phy_writes;
            uint32_t expected=0;
            for(unsigned i=0;i<4;i++)expected|=(uint32_t)row[3+i]<<(8*i);
            assert(reg==(unsigned)(row[0]|row[1]<<8) && width==row[2] && value==expected);
            assert(!bu_model_read(BU_CR,1) && bu_model_read(BU_TXPAUSE,1)==0xff &&
                (bu_model_read(2,1)&3)==3 && (bu_model_read(0x1f,1)&7)==7 &&
                (bu_model_read(BU_WLRF1,4)&BU_WLRF1_BBRF)==BU_WLRF1_BBRF);
            if(bu_model.phy_writes==2333)assert(bu_model.phy_delays==1);
            if(bu_model.phy_writes==2696)assert(bu_model.phy_delays==2);
            bu_model.phy_writes++;
        }
        if(reg==BU_CR) {
            unsigned previous=bu_model_read(BU_CR,1);
            if(width==2) {
                assert(!bu_model.rx_security_written && bu_model.h2c_posts==2 &&
                    bu_model_read(BU_CR,2)==0x049a && value==0x069a && bu_model_read(BU_TXPAUSE,1)==0xff &&
                    !bu_model.rx_samples && bu_model.rx_posts==4 && bu_rx_owner);
                bu_model.rx_security_written=true;
            } else if(value==0xbf) {
                assert(!bu_model.rx_dma_written && bu_model.rx_security_rcr_reloaded &&
                    bu_model_read(BU_CR,2)==0x069a && bu_model_read(BU_RX_SECCFG,2)==0x31cf &&
                    bu_model_read(BU_TXPAUSE,1)==0xff && !bu_model.rx_samples &&
                    bu_model.rx_posts==4 && bu_rx_owner);
                bu_model.rx_dma_written=true;
            } else if(value==0x9f) {
                assert(previous==0 && bu_model.rx_queue_mapped && !bu_model.rx_page_writes &&
                    bu_model_read(BU_TXPAUSE,1)==0xff && !bu_model.h2c_setup && bu_model.rx_posts==4);
            } else if(value==0x9a && previous==0x9f) {
                assert(bu_model.h2c_setup==6 && bu_model.rx_posts==4);
                if(bu_model.fault==BU_H2C_RESTORE_DRIFT)value=0x9f;
            } else assert(value==0 || value==5);
        }
        if(bu_model.rx_dma_written &&
           ((reg==0x10c && width==1) || reg==0x283 || reg==0x280)) {
            static const unsigned registers[]={0x10c,0x283,0x280};
            static const unsigned widths[]={1,1,2};
            static const unsigned control[]={0xa1,0,0x2003};
            static const unsigned original[]={0xa1,0x80,0x1234};
            unsigned i=bu_model.rx_fifo_writes++;
            assert(i<6 && reg==registers[i%3] && width==widths[i%3] &&
                   bu_model_read(BU_TXPAUSE,1)==0xff);
            if(!bu_model_read(BU_CR,1)) {
                assert(i>=3 && bu_model.rx_stops==1 && value==original[i%3]);
                if(reg==0x280 && bu_model.fault==BU_RX_FIFO_RESTORE_DRIFT)value^=1;
            } else assert(i<3 && value==control[i] && bu_model.rx_posts==4 && !bu_model.rx_samples);
        }
        if(reg==BU_RX_SECCFG) {
            unsigned i=bu_model.rx_security_policy_writes++;
            assert(i<2 && width==2 && bu_model.rx_security_written);
            if(!i) {
                assert(value==0x31cf && bu_model_read(BU_CR,2)==0x069a &&
                      bu_model_read(BU_TXPAUSE,1)==0xff && !bu_model.rx_security_rcr_reloaded);
                if(bu_model.fault==BU_RX_SEC_SETUP_DRIFT)value=0x3100;
            } else {
                assert(value==0x3100 && bu_model_read(BU_CR,1)==0 && bu_model.rx_stops==1 &&
                       bu_model.rx_security_cr_restored);
                if(bu_model.fault==BU_RX_SEC_RESTORE_DRIFT)value=0x31cf;
            }
        }
        if(reg==BU_CR+1 && bu_model.rx_security_written && !bu_model.rx_security_cr_restored) {
            assert(width==1 && value==4 && !bu_model_read(BU_CR,1) && bu_model.rx_stops==1);
            bu_model.rx_security_cr_restored=true;
        }
        if(reg==BU_H2C_STATUS && bu_model_read(BU_CR,1)==0x9f) {
            assert(width==4 && value==0x80000000u && bu_model.rx_queue_mapped &&
                !bu_model.rx_page_writes && !bu_model.h2c_status_writes++ && bu_model.h2c_status_reads==1);
        }
        if(bu_model_read(BU_CR,1)==0x9f &&
           (reg==0x244 || reg==0x24c || reg==0x248 || reg==0x254 || (reg==0x20d && (value&0x80)))) {
            static const unsigned registers[]={0x244,0x24c,0x248,0x254,0x254,0x20d};
            static const unsigned values[]={0x3fa00,0x3fa00,0x3fe00,0x09,0x0d,0x82};
            unsigned i=bu_model.h2c_setup++;
            assert(i<6 && reg==registers[i] && width==(i<3?4:1) && value==values[i]);
            assert(bu_model.h2c_status_writes==1 && bu_model.rx_llt_polls==1 && !bu_model.rx_filters_written);
            if(reg==BU_H2C_TAIL && bu_model.fault==BU_H2C_BAD_LIMIT)value^=0x80;
            if(i==5) {
                bu_model_store(BU_H2C_WP,4,BU_H2C_START);
                bu_model_store(BU_H2C_RP,4,BU_H2C_START+(bu_model.fault==BU_H2C_BAD_ENTRY?4:0));
                if(bu_model.fault!=BU_H2C_INIT_STUCK)bu_model_store(BU_H2C_INFO,1,0x09);
            }
        }
        if(bu_rx_owner && reg==0x10c && width==2) {
            assert(value==0xf5a0 && !bu_model_read(BU_CR,1) && !bu_model.rx_queue_mapped);
            bu_model.rx_queue_mapped=true;
        }
        if(bu_rx_owner && bu_model_read(BU_CR,1)==0x9f && reg>=0x230 && reg<=0x240) {
            static const unsigned pages[]={0x40,0x40,0x40,0,0x70b};
            unsigned i=bu_model.rx_page_writes++;
            assert(i<5 && reg==0x230+4*i && width==2 && value==pages[i]);
            assert(bu_model_read(BU_CR,1)==0x9f && bu_model_read(0x10c,2)==0xf5a1 &&
                   bu_model.h2c_status_writes==1 && bu_model.h2c_status_reads==2);
        }
        if(bu_rx_owner && bu_model_read(BU_CR,1)==0x9f && reg==BU_RX_LLT && (value&1)) {
            assert(width==1 && !bu_model.rx_llt_requested && bu_model.rx_page_writes==5 &&
                bu_model_read(0x11c,4)==0x5eff && bu_model_read(0x20b,1)==3 && (bu_model_read(0x20d,1)&2));
            bu_model.rx_llt_requested=true;
            if(bu_model.fault!=BU_RX_LLT_STUCK)value&=~1u;
        }
        if(bu_rx_owner && reg==0x608 && bu_model.rx_security_written) {
            assert(width==4 && value==0xf410400e && bu_model.rx_security_policy_writes==1 &&
                   bu_model_read(BU_CR,2)==0x069a && !bu_model.rx_security_rcr_reloaded);
            bu_model.rx_security_rcr_reloaded=true;
        } else if(bu_rx_owner && bu_model_read(BU_CR,1)==0x9f && reg==0x608) {
            assert(width==4 && value==0xf410400e && bu_model.rx_llt_polls==1 && !(bu_model_read(BU_RX_LLT,1)&1) &&
                bu_model_read(0x6a0,4)==0xffffffffu && bu_model_read(0x6a4,2)==0xffff &&
                bu_model.h2c_setup==6 && bu_model_read(BU_H2C_INFO,1)==0x09);
            bu_model.rx_filters_written=true;
            if(bu_model.fault==BU_RX_FILTER_DRIFT)value^=8;
        }
        if(reg==3 && (value&4) && bu_model.fault==BU_FW_FLASH)
            assert(!(bu_model_read(0x80,4)&(1u<<20)) && !(bu_model_read(0x40,4)&(1u<<19)));
        if(reg==0x80 && width==1 && bu_model_read(0x80,4)==0x80600001 && !value) {
            assert(bu_model_cold_board() && !bu_model.latch_clears++);
            assert(!(bu_model_read(3,1)&4) && !(bu_model_read(0x1d,1)&1) && !bu_model_read(0x100,1) &&
                !(bu_model_read(0x1208,4)>>31) && bu_model_read(0xec,4)==0x18000004);
        }
        assert(reg<BU_EFUSE_LDO || reg>BU_EFUSE_LDO+3);
        bu_model_store(reg,width,value);
        if(reg==0xc90 || reg==0xe90) {
            unsigned rf_reg=value>>20,path=reg==0xe90;
            unsigned direct=(path?0x2c00:0x2800)+4*rf_reg;
            if(bu_model.fault>=BU_RX_OK && bu_model.phy_writes==BU_PHY_ROWS && rf_reg==0x18) {
                unsigned expected=bu_model.channel_writes<2?0x50da1:0xc0a;
                assert((value&BU_RF18_TUNE)==expected && path==bu_model.channel_writes%2);
                bu_model.channel_writes++;
                if(path && bu_model.fault==BU_RX_TUNE_FAIL)value^=1;
            }
            bu_model_store(direct,4,value&BU_RF_MASK);
        }
        if(reg==0x28 && bu_model.fault==BU_PHY_BAD_CRYSTAL && bu_model.phy_writes==BU_PHY_ROWS)
            bu_model_store(reg,width,value^2);
        if(reg==0x1f && !value && bu_model.phy_started && bu_model.fault==BU_PHY_STUCK_RF)
            bu_model_store(reg,width,7);
        if(reg==BU_EFUSE_CTRL) {
            static const uint8_t efuse[]={0x0e,0x29,0x81,0xef,0x2e,0x34,0x28,0x2f,0x3d,3,0xff,0xff};
            unsigned address=bu_model.efuse_reads++;
            assert(bu_model.fault>=BU_BOARD_OK && bu_model.ddma==40 && width==4 &&
                address<BU_EFUSE_BYTES && value==(BU_EFUSE_READ_MODE|(address<<8)));
            assert(!bu_model_read(BU_CR,1) && !(bu_model_read(2,1)&3) &&
                !(bu_model_read(0x1f,1)&7) && !(bu_model_read(BU_WLRF1,4)&BU_WLRF1_BBRF));
            unsigned b=address<sizeof efuse?efuse[address]:0xff;
            if(address==9 && bu_model.fault==BU_BOARD_WRONG_RFE)b=5;
            if(bu_model.fault==BU_BOARD_WRONG_ADDR)value^=0x100;
            bu_model_store(reg,4,value|BU_EFUSE_READY|b);
        }
        if(reg==BU_FEATURE_MAILBOX && !value) {
            assert(bu_model.efuse_reads==BU_EFUSE_BYTES && !bu_model.feature_acks++);
            if(bu_model.fault==BU_BOARD_NO_ACK)bu_model_store(reg,1,BU_FEATURE_REPLY);
        }
        if(reg==5 && (value&1)) {
            bu_model_store(reg,1,value&~1u);
            bu_model_store(0x100,1,0);
        }
        if(reg==0x204)bu_model_store(reg,width,0);
        if(reg==0x1103 && bu_model.fault==BU_FW_PLATFORM_MCU)bu_model_store(0x80,2,0xc078);
        if(reg==0x1103 && (bu_model_cold_board() || bu_model.fault==BU_FW_COLD_DMA_BUSY))
            bu_model_store(0x80,4,0x80600001);
        if(reg==0x1700) {
            if(value==0xc00f0038 && bu_model.fault!=BU_FW_NOT_READY) {
                assert(bu_model.ddma==40 && bu_model.written==161176 && (bu_model_read(3,1)&4));
                bu_model_store(0x80,2,0xc078);
                bu_model_store(BU_CR+1,1,4);
                if(bu_model.fault==BU_RX_PAUSE_RESET || bu_model.fault==BU_RX_PAUSE_REFUSED)
                    bu_model_store(BU_TXPAUSE,1,0);
                if(bu_model.fault>=BU_BOARD_OK && bu_model.fault!=BU_BOARD_NO_REPLY)
                    bu_model_store(BU_FEATURE_MAILBOX,1,BU_FEATURE_REPLY);
            }
            bu_model_store(reg,width,value|0x20000000u);
        }
        if(reg==0x1208 && (value>>31)) {
            assert(bu_model.posted==bu_model.ddma+1);
            unsigned current=bu_model.ddma;
            unsigned count=bu_model_block_size(current);
            uint32_t dest=current<3?0x200000+bu_model.written:bu_model.written-11216;
            assert(bu_model_read(0x1200,4)==0x18780030 && bu_model_read(0x1204,4)==dest);
            assert(value==((current!=0 && current!=3?0xa1000000u:0xa0000000u)|count));
            bu_model.ddma++;bu_model.written+=count;
            if(bu_model.fault!=BU_FW_BUSY)bu_model_store(reg,4,(value&0x7fffffffu)|
                (bu_model.fault==BU_FW_CHECKSUM?0x08000000u:0));
        }
    }
    if(reg<=0xff || (reg>=0x1000 && reg<=0x10ff)) {
        bu_model.echo_expected=true;bu_model.echo=d->descriptor_buffer[0];
    }
    rtl_firmware_completion(strict_control.status,4,1,0);
    if(closing) {
        unsigned index=xhci.event_dequeue+1;
        struct xhci_trb control=xhci.event_ring[xhci.event_dequeue];
        if(index==XHCI_EVENT_TRB_COUNT){index=0;control.control^=1;}
        bu_model_receive_completion(true);
        xhci.event_ring[index]=control;
    }
}

static void bu_model_receive_event(bool closing,unsigned ahead) {
    struct bu_rx *rx=bu_rx_owner;
    unsigned n=(unsigned)((bu_model.rx_cursor-bu_model.rx_ring)/16);
    assert(rx && n<rx->posted && rx->completed<=n && rx->posted-rx->consumed<=4);
    assert((bu_model_read(BU_CR,2)==0x06bf ||
            (!bu_model.rx_samples && bu_model_read(BU_CR,1)==5)) && bu_model_read(BU_TXPAUSE,1)==0xff);
    uint64_t pointer=bu_model.rx_cursor&~1ull;
    const struct xhci_trb *trb=(const void *)(uintptr_t)pointer;
    uint64_t phys=trb->parameter_low|(uint64_t)trb->parameter_high<<32;
    assert(trb->status==32768 && (trb->control&1) && (phys&0xffff)+32768<=0x10000);
    assert(phys==rx->phys+(n%4)*32768);
    uint8_t *data=(void *)(uintptr_t)phys;
    memset(data,0,80);
    data[0]=54;
    data[24]=0x80;
    memcpy(data+60,"\x00\x09" "Demers2.4" "\x03\x01\x0a",14);
    if(!n && !closing)data[1]=0x40;
    unsigned actual=78;
    if(bu_model.fault==BU_RX_WRONG_EVENT || bu_model.fault==BU_RX_EARLY_STALE)pointer+=16;
    else if(ahead && bu_model.fault==BU_RX_DEPTH_DUPLICATE)pointer-=16;
    else {
        bu_model.rx_cursor+=16;
        context_write32(rx->device->output_context,BU_RX_DCI,2,(uint32_t)bu_model.rx_cursor);
        context_write32(rx->device->output_context,BU_RX_DCI,3,bu_model.rx_cursor>>32);
    }
    unsigned index=xhci.event_dequeue+ahead;
    bool cycle=xhci.event_cycle;
    if(index==XHCI_EVENT_TRB_COUNT){index=0;cycle=!cycle;}
    xhci.event_ring[index]=(struct xhci_trb){.parameter_low=(uint32_t)pointer,
        .parameter_high=pointer>>32,.status=(13u<<24)|(BU_RX_BYTES-actual),
        .control=(32u<<10)|(4u<<24)|(BU_RX_DCI<<16)|cycle};
}

static void bu_model_receive_completion(bool closing) {
    bu_model_receive_event(closing,0);
}

static void bu_model_receive(void) {
    struct bu_rx *rx=bu_rx_owner;
    unsigned n=bu_model.rx_posts++;
    assert(rx && rx->posted==n+1 && rx->posted-rx->consumed<=4);
    assert(bu_model_read(BU_TXPAUSE,1)==0xff);
    if(n<4) {
        assert(!bu_model.rx_samples && !bu_model.ddma && !bu_model.posted && !bu_model_read(BU_CR,1));
    } else {
        assert(bu_model.rx_samples==10 && bu_model.rx_filters_written &&
               bu_model_read(BU_CR,2)==0x06bf && bu_model_read(0x608,4)==0xf410400e);
        assert(bu_model.h2c_posts==2 && bu_model_read(BU_H2C_WP,4)==BU_H2C_START+64 &&
               bu_model_read(BU_H2C_RP,4)==BU_H2C_START+64);
    }
    const struct xhci_trb *td=(const void *)(uintptr_t)(bu_model.rx_ring-1+n*16);
    uint64_t phys=td->parameter_low|(uint64_t)td->parameter_high<<32;
    assert(td->status==32768 && phys==rx->phys+(n%4)*32768 && !(phys&32767));
}

static void bu_model_depth_tick(void) {
    if(bu_model.fault!=BU_RX_OK && bu_model.fault!=BU_RX_DEPTH_OK && bu_model.fault!=BU_RX_DEPTH_DUPLICATE &&
       bu_model.fault!=BU_RX_WRONG_EVENT && bu_model.fault!=BU_RX_PAUSE_RESET)return;
    if(bu_model.rx_samples==10 && !strict_control.device && bu_rx_owner &&
       bu_rx_owner->posted==4 && !bu_model.rx_depth_event) {
        bu_model.rx_depth_event=true;
        bu_model_receive_event(false,0);
        if(bu_model.fault!=BU_RX_WRONG_EVENT)bu_model_receive_event(false,1);
    }
}

static void bu_model_mmio(uint64_t offset,uint32_t value) {
    struct xhci_device *d=&xhci.devices[3];
    if(offset==xhci.db_base && !value)bu_model_command(d);
    else if(offset==xhci.db_base+16 && value==10) {
        if(bu_model.ddma==40)bu_model_h2c(d);
        else bu_model_bulk(d);
    }
    else if(offset==xhci.db_base+16 && value==BU_RX_DCI)bu_model_receive();
    else if(offset==xhci.db_base+16 && value==1)bu_model_control();
    else rtl8822bu_probe_model(offset,value);
}

static bool bu_model_delay(uint64_t us) {
    assert(us==1000 || us==50 || us==200000 || (us==13 && bu_model.fault>=BU_RX_OK));
    if(us==200000) {
        assert(bu_model.phy_started && (bu_model.phy_writes==2333 || bu_model.phy_writes==2696));
        bu_model.phy_delays++;
    }
    test_time+=us;
    return true;
}

static void test_rtl8822bu_firmware(const char *path,const char *phy_path,bool passive) {
    static uint8_t image[BU_FW_BYTES];
    static uint8_t phy[BU_PHY_BYTES];
    FILE *file=fopen(path,"rb");
    assert(file && fread(image,1,sizeof image,file)==sizeof image && fgetc(file)==EOF && !fclose(file));
    assert(bu_image_valid(image,sizeof image));
    if(phy_path) {
        file=fopen(phy_path,"rb");
        assert(file && fread(phy,1,sizeof phy,file)==sizeof phy && fgetc(file)==EOF && !fclose(file));
        assert(bu_phy_profile_valid(phy,sizeof phy));
    }
    for(enum bu_firmware_fault fault=BU_FW_OK;fault<=(passive?BU_H2C_RESTORE_DRIFT:phy_path?BU_PHY_STUCK_RF:BU_BOARD_BAD_STOP);fault++) {
        struct xhci_device *rtl=rtl_hub_fixture(),*d=&xhci.devices[3];
        d->device_bcd=0x0210;d->configuration_count=1;d->ep0_max_packet=64;
        uint8_t *command_memory;
        assert(allocate_device_runtime(d) && alloc_frame_zero(&xhci.command_ring_phys,(void **)&command_memory));
        xhci.command_ring=(void *)command_memory;xhci.command_cycle=true;
        test_time_step=1;bu_probe_case=BU_OK;bu_leaf_requests=bu_hub_reads=0;
        native_boundary_mmio_hook=rtl8822bu_probe_model;
        assert(xhci_rtl8822bu_capture_identity());
        memset(&bu_model,0,sizeof bu_model);bu_model.image=image;bu_model.phy=phy;bu_model.fault=fault;
        bu_model_store(0x100,1,fault==BU_FW_WARM?0:0xea);
        bu_model_store(0x6,1,3);bu_model_store(0x80,2,0x78);
        bu_model_store(0xec,4,0x1f000004);
        if((fault>=BU_FW_COLD_LATCH && fault<=BU_FW_OTHER_STATUS) || bu_model_cold_board()) {
            bu_model_store(0xf5,1,fault==BU_FW_OTHER_STATUS?0x11:0x10);
            bu_model_store(0x80,4,fault==BU_FW_OTHER_LATCH?0x00600003:0x00600001);
            bu_model_store(0x1d,1,1);
        }
        if(fault==BU_FW_COLD_DMA_BUSY)bu_model_store(0x1208,4,0x80000000);
        if(fault==BU_FW_FLASH) {
            bu_model_store(0x80,4,0x00100078);
            bu_model_store(0x40,4,0x00080200);
        }
        if(fault==BU_FW_ENTRY_MCU)bu_model_store(0x80,2,0xc078);
        bu_model_store(0x3,1,0xd8);bu_model_store(0x550,1,0x14);
        bu_model_store(0x254,1,8);
        bu_model_store(BU_RX_SECCFG,2,0x3100);
        bu_model_store(BU_RX_MAC_REPORT+3,1,fault==BU_RX_MAC_RESET_BUSY?0x5e:0x56);
        bu_model_store(0x280,4,0x80001234u);
        bu_model_store(0x1700,4,0x20000000);
        bu_model_store(0x1708,4,0xdd03);
        bool board=fault>=BU_BOARD_OK;
        if(board) {
            bu_model_store(BU_EFUSE_CTRL,4,fault==BU_BOARD_BAD_MODE?0x906000ff:0xb163f0ff);
            bu_model_store(BU_EFUSE_LDO,4,fault==BU_BOARD_BAD_BANK?0x98000100:0x18000000);
            bu_model_store(BU_FEATURE_MAILBOX+6,4,fault==BU_BOARD_WRONG_FEATURE?0xff37ff72:0xff57ff72);
        }
        rtl_test_clock=true;rtl_test_delay=bu_model_delay;
        if(fault==BU_FW_MISSING)test_time_step=10000;
        native_boundary_mmio_hook=bu_model_mmio;
        native_boundary_clock_hook=bu_model_depth_tick;
        if(fault==BU_FW_IMAGE)image[0]^=1;
        bool safe=false;
        bool success=fault==BU_FW_OK || fault==BU_FW_FLASH || fault==BU_FW_COLD_LATCH ||
            fault==BU_BOARD_OK || fault==BU_PHY_OK || fault==BU_RX_OK || fault==BU_RX_DEPTH_OK ||
            fault==BU_RX_PAUSE_RESET;
        if(fault>=BU_RX_OK) {
            test_console_printf_hook=bu_model_out_log;
            assert(xhci_rtl8822bu_capture_passive(image,sizeof image,phy,sizeof phy,&safe)==success);
            test_console_printf_hook=NULL;
            assert(!bu_rx_owner && !bu_model_read(BU_CR,1) && (bu_model_read(0x1f,1)&7)==0);
            assert(bu_model.channel_writes==(fault==BU_RX_EP_AVG_DRIFT || fault==BU_RX_EARLY_STALE ||
                fault==BU_RX_EARLY_FW_SHORT || fault==BU_RX_PAUSE_REFUSED?0:fault==BU_RX_TUNE_FAIL?2:4));
            bool armed=fault!=BU_RX_EP_AVG_DRIFT;
            bool uploaded=armed && fault!=BU_RX_EARLY_FW_SHORT;
            bool board_ready=uploaded && fault!=BU_RX_EARLY_STALE;
            bool firmware_ready=board_ready && fault!=BU_RX_TUNE_FAIL && fault!=BU_RX_LLT_STUCK &&
                fault!=BU_RX_FILTER_DRIFT && fault!=BU_RX_PAUSE_REFUSED && fault<BU_H2C_BAD_LIMIT;
            bool dma_ready=firmware_ready && fault!=BU_RX_SEC_SETUP_DRIFT;
            bool admitted=dma_ready && fault!=BU_RX_FIFO_BOUNDARY_DRIFT;
            unsigned extra=(fault==BU_RX_OK || fault==BU_RX_DEPTH_OK || fault==BU_RX_DEPTH_DUPLICATE ||
                            fault==BU_RX_DEPTH_TRANSITION || fault==BU_RX_EARLY_VALID || fault==BU_RX_PAUSE_RESET);
            assert(bu_model.rx_posts==(armed?4u+extra:0u));
            assert(bu_model.pause_writes==(fault==BU_RX_PAUSE_REFUSED?3u:fault==BU_RX_PAUSE_RESET?2u:1u));
            unsigned commands=firmware_ready || fault>=BU_H2C_UNDRAINED?2:
                fault>=BU_H2C_OUT_SHORT && fault<=BU_H2C_OUT_WRONG_TRB?1:0;
            assert(bu_model.h2c_posts==commands && !d->bulk_pending);
            assert(bu_model.out_event_logs==(unsigned)uploaded+commands);
            assert(bu_model.out_state_logs==3u+uploaded+2*commands);
            assert(bu_model.out_device_logs==(fault>=BU_H2C_OUT_SHORT && fault<=BU_H2C_OUT_WRONG_TRB));
            bool queue_setup=board_ready && fault!=BU_RX_TUNE_FAIL && fault!=BU_RX_PAUSE_REFUSED;
            assert(bu_model.h2c_status_writes==queue_setup);
            assert(bu_model.h2c_status_reads==2*bu_model.h2c_status_writes+bu_model.out_device_logs);
            assert(bu_model.rx_filters_written==(queue_setup && fault!=BU_RX_LLT_STUCK &&
                fault!=BU_H2C_BAD_LIMIT && fault!=BU_H2C_INIT_STUCK));
            assert(bu_model.rx_stops==1);
            assert(bu_model.rx_samples==(admitted && fault!=BU_RX_MAC_RESTORE_DRIFT?20:0));
            unsigned mac_writes=admitted?10u:0u,mac_reads=admitted?8u:0u;
            if(fault==BU_RX_MAC_SELECTOR_DRIFT){mac_writes=4;mac_reads=2;}
            if(fault==BU_RX_MAC_RESET_BUSY){mac_writes=0;mac_reads=0;}
            if(fault==BU_RX_MAC_RESTORE_DRIFT){mac_writes=5;mac_reads=4;}
            assert(bu_model.rx_mac_writes==mac_writes && bu_model.rx_mac_reads==mac_reads);
            unsigned mac_selector=fault==BU_RX_MAC_RESET_BUSY?0x5e:fault==BU_RX_MAC_RESTORE_DRIFT?0x46:0x56;
            assert(bu_model_read(BU_RX_MAC_REPORT+3,1)==mac_selector);
            assert(bu_model.rx_security_written==firmware_ready);
            assert(bu_model.rx_dma_written==dma_ready);
            unsigned fifo_writes=admitted?6u:0u;
            if(fault==BU_RX_BAD_STOP)fifo_writes-=3;
            assert(bu_model.rx_fifo_writes==fifo_writes);
            assert(bu_model_read(0x280,4)==(fault==BU_RX_BAD_STOP?0x2003u:
                fault==BU_RX_FIFO_RESTORE_DRIFT?0x80001235u:0x80001234u));
            assert(bu_model.rx_security_policy_writes==2u*bu_model.rx_security_written &&
                   bu_model.rx_security_rcr_reloaded==bu_model.rx_security_written &&
                   bu_model.rx_security_cr_restored==bu_model.rx_security_written);
            assert(bu_model_read(BU_RX_SECCFG,2)==(fault==BU_RX_SEC_RESTORE_DRIFT?0x31cfu:0x3100u));
            assert(bu_model.rx_depth_event==(fault==BU_RX_OK || fault==BU_RX_DEPTH_OK ||
                fault==BU_RX_DEPTH_DUPLICATE || fault==BU_RX_WRONG_EVENT || fault==BU_RX_PAUSE_RESET));
            assert(bu_model.rx_llt_polls==(!queue_setup?0:fault==BU_RX_LLT_STUCK?100:1));
            if(fault==BU_RX_CLOSING)assert(bu_model.rx_cursor==bu_model.rx_ring+16);
        } else if(fault>=BU_PHY_OK) {
            if(fault==BU_PHY_BAD_PROFILE)phy[2]^=1;
            assert(xhci_rtl8822bu_capture_phy(image,sizeof image,phy,sizeof phy,&safe)==success);
            if(fault==BU_PHY_BAD_PROFILE)phy[2]^=1;
            assert(bu_model.phy_started==(fault!=BU_PHY_BAD_PROFILE));
            assert(bu_model.phy_writes==(bu_model.phy_started?BU_PHY_ROWS:0));
            assert(bu_model.phy_delays==(bu_model.phy_started?2:0));
            assert((bu_model_read(0x1f,1)&7)==(fault==BU_PHY_STUCK_RF?7:0));
        } else {
            assert((board?xhci_rtl8822bu_capture_board:xhci_rtl8822bu_capture_firmware)
                (image,sizeof image,&safe)==success);
        }
        if(fault==BU_FW_IMAGE)image[0]^=1;
        assert(safe==(fault!=BU_FW_BAD_STOP && fault!=BU_BOARD_BAD_STOP && fault!=BU_FW_BUSY && fault!=BU_PHY_STUCK_RF &&
            fault!=BU_FW_PLATFORM_MCU && fault!=BU_FW_COLD_DMA_BUSY && fault!=BU_RX_BAD_STOP &&
            fault!=BU_RX_SEC_RESTORE_DRIFT && fault!=BU_RX_FIFO_RESTORE_DRIFT && fault!=BU_RX_PAUSE_REFUSED));
        assert(!bu_model.echo_expected && !rtl->control_success && !xhci.devices[2].control_success && !network_count);
        if(success)assert(bu_model.posted==40 && bu_model.ddma==40 && !d->bulk_pending && bu_model.stop);
        if(fault==BU_FW_IMAGE)assert(!bu_model.posted && !bu_model.configure && !bu_model.echoes);
        if(fault==BU_FW_WARM || fault==BU_FW_ENTRY_MCU)assert(!bu_model.posted && !bu_model.register_writes);
        if(fault==BU_FW_OTHER_LATCH || fault==BU_FW_OTHER_STATUS)assert(!bu_model.posted && !bu_model.register_writes);
        if(fault==BU_FW_COLD_DMA_BUSY)assert(!bu_model.posted && bu_model_read(0x80,4)==0x80600001);
        assert(bu_model.latch_clears==bu_model_cold_board());
        if(fault==BU_FW_PLATFORM_MCU)assert(!bu_model.posted && bu_model_read(0x80,2)==0xc078);
        if(fault==BU_FW_SHORT || fault==BU_FW_MISSING)assert(bu_model.posted==1 && !bu_model.ddma && !d->bulk_pending);
        if(fault==BU_RX_EP_AVG_DRIFT || fault==BU_RX_EARLY_FW_SHORT || fault==BU_RX_EARLY_STALE) {
            assert(!bu_model.feature_acks && !bu_model.efuse_reads && bu_model.stop==1);
            assert(bu_model.posted==(fault==BU_RX_EARLY_STALE?40u:fault==BU_RX_EARLY_FW_SHORT?1u:0u));
        } else if(board) {
            unsigned reads=BU_EFUSE_BYTES;
            if(fault==BU_BOARD_BAD_BANK || fault==BU_BOARD_BAD_MODE)reads=0;
            if(fault==BU_BOARD_WRONG_ADDR)reads=1;
            bool ack=fault==BU_BOARD_OK || fault==BU_BOARD_WRONG_FEATURE ||
                fault==BU_BOARD_NO_ACK || fault==BU_BOARD_BAD_STOP || fault>=BU_PHY_OK;
            assert(bu_model.posted==40 && bu_model.stop==1 && bu_model.efuse_reads==reads && bu_model.feature_acks==ack);
        }
    }
    uint8_t logical[BU_EFUSE_LOGICAL_BYTES];
    assert(!bu_decode_efuse((uint8_t[]){0xef},1,logical));
    assert(!bu_decode_efuse((uint8_t[]){0x0e,0x29},2,logical));
    assert(!bu_decode_efuse((uint8_t[]){0x0f,0xce,0,0,0xff},5,logical));
    assert(bu_decode_efuse((uint8_t[]){0x0e,0x29,0x81,0x0e,0x01,0x02,0xff},7,logical));
    assert(logical[0]==1 && logical[1]==2 && logical[2]==0xff);
    native_boundary_mmio_hook=NULL;native_boundary_clock_hook=NULL;rtl_test_delay=NULL;rtl_test_clock=false;
    puts("rtl8822bu: PASS native firmware upload, forty USB packets, both DDMA sections, MCU ready and endpoint retirement");
    puts("rtl8822bu: PASS cold flash selection, exact WLRF1 bit isolation and active MCU refusal");
    puts("rtl8822bu: PASS captured cold latch, CPU and DDMA ownership checks, and mismatched signature refusals");
    puts("rtl8822bu: PASS image and warm refusal, short/missing OUT, checksum, missing ready, wrong stop and busy DDMA");
    puts("rtl8822bu: PASS native board eFuse and firmware reply, no RF/program power, stale address and bad board/mailbox refusals");
    if(phy_path)puts("rtl8822bu: PASS native RFE3 tables, two RF delays, TX inhibited, profile and crystal refusal, and RF retirement");
    if(passive)puts("rtl8822bu: PASS native tuning, 32 KiB bulk IN, beacon parsing, stale/timeout and retirement refusal");
    if(passive)puts("rtl8822bu: PASS read-only pre/post PHY and DMA counters and closing completion excluded from discovery");
    if(passive)puts("rtl8822bu: PASS restoring TX pause after MCU startup and refusing unverified pause");
    if(passive)puts("rtl8822bu: PASS MAC counters, unavailable and mismatched samples, selector restoration and restore refusal");
    if(passive)puts("rtl8822bu: PASS Linux RX admission order, MAC TX disabled, LLT completion, and filter readback refusal");
    if(passive)puts("rtl8822bu: PASS Netgear four bulk-IN owners before firmware, startup discard, stale event refusal, and early cleanup");
    if(passive)puts("rtl8822bu: PASS native firmware commands, Linux USB bytes, queue entry/drain, short/missing OUT and RX-only restoration");
    if(passive)puts("rtl8822bu: PASS Netgear OUT ring and completion tracing, stale running context, and wrong-TRB refusal");
    if(passive)puts("rtl8822bu: PASS Netgear queue setup order, unavailable status, boundary drift and stuck initialization");
}
