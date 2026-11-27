#include "fixtures/wpa2-handshake-synthetic.h"

enum rtl_station_fixture {
    RTL_STATION_OFF,RTL_STATION_OK,RTL_STATION_BAD_MIC,RTL_STATION_USB_TIMEOUT,RTL_STATION_USB_SPIN_LIMIT,
    RTL_STATION_USB_LATE,RTL_STATION_USB_BAD_CODE,RTL_STATION_USB_RESIDUE,RTL_STATION_USB_WRONG_TRB,
    RTL_STATION_USB_EVENT_DATA,RTL_STATION_QUEUE_LOAD,RTL_STATION_QUEUE_DRIFT,RTL_STATION_SIE_MAP,
    RTL_STATION_OUT_HALTED,RTL_STATION_OUT_BAD_CONTEXT,RTL_STATION_DMA_READBACK,RTL_STATION_DMA_PAUSE,
    RTL_STATION_DMA_FIRMWARE,RTL_STATION_NO_SIBLING,
};
static enum rtl_station_fixture rtl_station_fixture;
static bool rtl_station_test_m1;
#ifdef SRVROS_RTL8188CU_IP_TEST
static bool rtl_station_ip_fixture;
static bool rtl_station_no_offer;
static bool rtl_station_desktop_fixture;
static enum rtl_cu_ccx_result rtl_station_report_fixture;
static void rtl_test_ap_tx(const uint8_t *,size_t);
static void rtl_test_ap_rx(unsigned);
static bool rtl_test_desktop_begin(void *);
static bool rtl_test_desktop_running(void *);
#endif

static bool rtl_station_early_dma_failure(void) {
    return rtl_station_fixture>=RTL_STATION_DMA_READBACK && rtl_station_fixture<=RTL_STATION_DMA_FIRMWARE;
}

static void rtl_radio_out_event(unsigned offset,uint64_t trb,unsigned code,unsigned residue,bool event_data) {
    rtl_radio_transfer_event(offset,trb,code,residue);
    unsigned index=(xhci.event_dequeue+offset)%XHCI_EVENT_TRB_COUNT;
    xhci.event_ring[index].control^=(3u^RTL_CU_TX_DCI)<<16;
    if(event_data)xhci.event_ring[index].control|=4;
}

static void rtl_radio_station_rx_complete(void) {
    if(rtl_radio_model.rx_index==XHCI_TRB_COUNT-1) {
        const struct xhci_trb *link=&rtl_passive.ep.ring[XHCI_TRB_COUNT-1];
        assert(link->control==((XHCI_TRB_TYPE_LINK<<10)|2u|rtl_radio_model.rx_cycle));
        rtl_radio_model.rx_index=0;
        rtl_radio_model.rx_cycle=!rtl_radio_model.rx_cycle;
        rtl_radio_model.rx_wraps++;
    }
    assert(rtl_passive.trb==rtl_passive.ep.ring_phys+rtl_radio_model.rx_index*16 &&
           rtl_passive.trb_cycle==rtl_radio_model.rx_cycle);
    rtl_radio_model.rx_index++;
}

bool rtl_cu_station_test_nonce(uint8_t out[32]) {
#ifdef SRVROS_RTL8188CU_IP_TEST
    if(rtl_station_desktop_fixture)return stack_test_nonce(out);
#endif
    memcpy(out,hs_sn,sizeof hs_sn);
    return true;
}

static void rtl_radio_station_response(unsigned offset,unsigned subtype,const uint8_t *eapol,size_t length) {
    assert(rtl_passive.pending);
    uint8_t *frame=rtl_passive.buffer+24;
    memset(frame,0,200);
    memcpy(frame+4,hs_spa,6);
    memcpy(frame+10,hs_aa,6);
    memcpy(frame+16,hs_aa,6);
    frame[0]=(uint8_t)subtype;
    size_t size=30;
    if(eapol) {
        static const uint8_t llc[]={0xaa,0xaa,3,0,0,0,0x88,0x8e};
        frame[1]=0x0a; /* From DS and retry, as observed on M3. */
        frame[24]=7;
        memcpy(frame+26,llc,sizeof llc);
        memcpy(frame+34,eapol,length);
        size=34+length;
        if(eapol==hs_m3 && rtl_station_fixture==RTL_STATION_BAD_MIC)frame[34+81]^=1;
    } else if(subtype==0xb0) {
        frame[26]=2;
    } else {
        frame[24]=0x11;
        frame[28]=23;
        frame[29]=0xc0;
        const uint8_t ies[]={1,4,0x82,4,11,22,221,7,0,0x50,0xf2,2,0,1,0};
        memcpy(frame+size,ies,sizeof ies);
        size+=sizeof ies;
    }
    size+=4;
    rtl_passive.buffer[0]=(uint8_t)size;
    rtl_passive.buffer[1]=size>>8;
    rtl_radio_transfer_event(offset,rtl_passive.trb,13,4096-24-size);
    rtl_radio_station_rx_complete();
}

static bool rtl_radio_station_doorbell(uint64_t value) {
    static const uint8_t frame_control[]={0xb0,0,0x88,0x88,0xc0};
    if(value==RTL_CU_TX_DCI) {
        assert(rtl_station_fixture!=RTL_STATION_OFF && rtl_station.pending);
        assert(rtl_radio_cr()==0x02ff && !rtl_radio_model.reg[0x522]);
        assert(rtl_radio_model.tx_queue_step==10 && rtl_radio_model.out_status_reads==rtl_station.generation &&
               rtl_station.queue_ready && rtl_station.out_status_valid && !rtl_station.out_status);
        const struct xhci_trb *trb=(const struct xhci_trb *)(uintptr_t)rtl_station.trb;
        assert(((uint64_t)trb->parameter_high<<32|trb->parameter_low)==rtl_station.physical);
        assert((trb->control&1)==rtl_station.trb_cycle);
#ifdef SRVROS_RTL8188CU_IP_TEST
        if(rtl_station_desktop_fixture) {
            struct xhci_device *d=rtl_passive.owner;
            uint64_t next=(rtl_station.ep.ring_phys+rtl_station.ep.enqueue*16)|rtl_station.ep.cycle;
            context_write32(d->output_context,RTL_CU_TX_DCI,2,(uint32_t)next);
            context_write32(d->output_context,RTL_CU_TX_DCI,3,next>>32);
        }
#endif
        unsigned sent=rtl_station.sends;
        const uint8_t *packet=rtl_station.buffer;
        assert(sent>0);
#ifdef SRVROS_RTL8188CU_IP_TEST
        if(rtl_station_ip_fixture && rtl_ip.live) {
            bool first=rtl_ip.posted==1;
            assert(!!(packet[10]&8)==(first && rtl_station_report_fixture!=RTL_CU_CCX_UNAVAILABLE));
            rtl_radio_out_event(0,rtl_station.trb,1,0,false);
            if(first && rtl_station_report_fixture!=RTL_CU_CCX_MISSING &&
               rtl_station_report_fixture!=RTL_CU_CCX_UNAVAILABLE) {
                rtl_radio_model.reg[RTL_CU_C2H_OWNER]=0xff;
                rtl_radio_model.reg[RTL_CU_C2H_EVENT]=rtl_station_report_fixture==RTL_CU_CCX_OTHER_EVENT?0x81:0x83;
                rtl_radio_model.reg[RTL_CU_C2H_EVENT+2]=2;
                rtl_radio_model.reg[RTL_CU_C2H_EVENT+9]=rtl_station_report_fixture==RTL_CU_CCX_ACKED?RTL_CU_CCX_ACK:0;
            }
            rtl_test_ap_tx(packet,trb->status);
            rtl_test_ap_rx(1);
            return true;
        }
#endif
        assert(sent<=5 || rtl_station.ip_requested);
        assert(trb->status==(unsigned)(packet[0]|packet[1]<<8)+32 && packet[2]==32 && packet[3]==0x8c);
        unsigned check=0;
        for(unsigned i=0;i<32;i+=2)check^=packet[i]|(unsigned)packet[i+1]<<8;
        assert(!check && packet[14]==(uint8_t)rtl_station.sequence && packet[17]==1);
        assert(!memcmp(packet+36,hs_aa,6) && !memcmp(packet+42,hs_spa,6) && !memcmp(packet+48,hs_aa,6));
        bool leaving=packet[32]==0xc0;
        if(leaving)assert(sent==5 || (sent==4 && rtl_station_fixture==RTL_STATION_BAD_MIC) || rtl_station.ip_requested);
        else assert(packet[32]==frame_control[sent-1]);
        assert(packet[5]==((!leaving && (sent==3 || sent==4))?7:0x12));
        if(rtl_station.generation==1 && !leaving && sent==3)assert(!memcmp(packet+66,hs_m2,sizeof hs_m2));
        if(rtl_station.generation==1 && !leaving && sent==4)assert(!memcmp(packet+66,hs_m4,sizeof hs_m4));
        if(rtl_station_fixture>=RTL_STATION_USB_TIMEOUT && rtl_station_fixture<=RTL_STATION_USB_LATE) {
            assert(sent==1);
            test_time_step=rtl_station_fixture==RTL_STATION_USB_SPIN_LIMIT?1:300000;
            return true;
        }
        if(rtl_station_fixture>=RTL_STATION_USB_BAD_CODE && rtl_station_fixture<=RTL_STATION_USB_EVENT_DATA) {
            assert(sent==1);
            bool bad_code=rtl_station_fixture==RTL_STATION_USB_BAD_CODE;
            if(bad_code)context_write32(rtl_passive.owner->output_context,RTL_CU_TX_DCI,0,2);
            rtl_radio_out_event(0,rtl_station.trb+(rtl_station_fixture==RTL_STATION_USB_WRONG_TRB?16:0),
                bad_code?4:1,rtl_station_fixture==RTL_STATION_USB_RESIDUE?7:0,
                rtl_station_fixture==RTL_STATION_USB_EVENT_DATA);
            return true;
        }
        rtl_radio_out_event(0,rtl_station.trb,1,0,false);
        if(sent==1)rtl_radio_station_response(1,0xb0,NULL,0);
        if(sent==2)rtl_radio_station_response(1,0x10,NULL,0);
        if(sent==3 && !leaving) {
#ifdef SRVROS_RTL8188CU_IP_TEST
            if(rtl_station_desktop_fixture) {
                uint8_t m3[sizeof hs_m3];
                size_t bytes=0;
                assert(stack_test_message3(m3,sizeof m3,&bytes) && bytes==sizeof m3);
                rtl_radio_station_response(1,0x88,m3,bytes);
            } else
#endif
                rtl_radio_station_response(1,0x88,hs_m3,sizeof hs_m3);
        }
        return true;
    }
    if(value==RTL_CU_RX_DCI && rtl_station.enabled) {
#ifdef SRVROS_RTL8188CU_IP_TEST
        if(rtl_station_ip_fixture && rtl_ip.live) {
            rtl_test_ap_rx(0);
            return true;
        }
#endif
        if(rtl_station.aid && !rtl_station_test_m1) {
            rtl_station_test_m1=true;
            rtl_radio_station_response(0,0x88,hs_m1,sizeof hs_m1);
        }
        return true;
    }
    return false;
}

static void rtl_radio_assert_auth_trace(void) {
    const struct rtl_cu_auth_trace *trace=&rtl_station.auth_out;
    if(rtl_station_fixture==RTL_STATION_NO_SIBLING) {
        assert(!rtl_station.early_tx_dma && !rtl_station.queue_attempted && !rtl_station.ready && !rtl_station.sends &&
               !rtl_station.enabled && !trace->trb);
        return;
    }
    assert(rtl_station.early_tx_dma && rtl_station.early_pause_valid && rtl_passive.early_rx_reads==2);
    if(rtl_station_early_dma_failure()) {
        bool firmware_drift=rtl_station_fixture==RTL_STATION_DMA_FIRMWARE;
        assert(!rtl_station.sends && !trace->trb && !rtl_station.ready &&
               rtl_station.queue_attempted==firmware_drift && rtl_station.queue_ready==firmware_drift);
        assert(rtl_station.early_pause==(rtl_station_fixture==RTL_STATION_DMA_PAUSE?0x7f:0xff));
        assert(rtl_passive.early_rx_after==(rtl_station_fixture==RTL_STATION_DMA_READBACK?0x02bb:0x02bf));
        assert(rtl_passive.linked==(firmware_drift?256u:0u) && rtl_fw_model.arm_writes==(unsigned)firmware_drift);
        return;
    }
    assert(rtl_passive.early_rx_after==0x02bf && rtl_station.early_pause==0xff &&
           rtl_passive.early_rx_us && rtl_passive.early_rx_us<rtl_passive.llt_started_us);
    if(rtl_station_fixture>=RTL_STATION_QUEUE_LOAD) {
        assert(rtl_station.queue_attempted && !rtl_station.ready && !rtl_station.sends && !trace->trb);
        if(rtl_station_fixture==RTL_STATION_QUEUE_LOAD) {
            assert(rtl_station.queue_load==0x80e70009 && !rtl_station.queue_ready &&
                   rtl_radio_model.tx_queue_step==2 && !rtl_fw_model.arm_writes && !rtl_radio_model.commands);
            return;
        }
        assert(rtl_station.queue_ready && rtl_station.early_queue.reads==8 && rtl_station.auth_queue.reads==8);
        assert(rtl_station.auth_queue.value[RTL_CU_TX_CR_SAMPLE]==0x02ff);
        bool out_checked=rtl_station_fixture==RTL_STATION_OUT_HALTED || rtl_station_fixture==RTL_STATION_OUT_BAD_CONTEXT;
        assert(rtl_station.out_status_valid==out_checked && rtl_radio_model.out_status_reads==(unsigned)out_checked);
        if(rtl_station_fixture==RTL_STATION_QUEUE_DRIFT)
            assert(rtl_station.auth_queue.value[RTL_CU_TX_PAGES_SAMPLE]==0x00e70008);
        if(rtl_station_fixture==RTL_STATION_SIE_MAP)
            assert(rtl_station.auth_queue.value[RTL_CU_TX_SIE_HIGH_NORMAL_SAMPLE]==0x23);
        if(out_checked) {
            assert(rtl_station.out_status==(rtl_station_fixture==RTL_STATION_OUT_HALTED?1u:0u));
            assert((rtl_station.out_info>>16)==(rtl_station_fixture==RTL_STATION_OUT_BAD_CONTEXT?64u:512u));
        }
        return;
    }
    assert(trace->slot==2 && trace->length==62 && trace->elapsed_us);
    assert(trace->before.sampled && trace->before.state==1);
    if(rtl_station.desktop && rtl_station.generation>1)
        assert((trace->before.dequeue&~1ull)==trace->trb && trace->trb!=rtl_station.ep.ring_phys);
    else
        assert(trace->trb==rtl_station.ep.ring_phys && trace->before.dequeue==(trace->trb|1));
    bool bad_code=rtl_station_fixture==RTL_STATION_USB_BAD_CODE;
    assert(trace->wait_end.sampled && trace->wait_end.state==(bad_code?2u:1u));
    assert(trace->stop_end.sampled && trace->stop_end.state==3);
    if(rtl_station_fixture==RTL_STATION_OK || rtl_station_fixture==RTL_STATION_BAD_MIC) {
        assert(!strcmp(trace->outcome,"complete") && trace->wait.seen && trace->wait.code==1 &&
               trace->wait.trb==trace->trb && !trace->wait.residue && !trace->wait.event_data);
        assert(!trace->late.seen && !trace->stop.seen);
        return;
    }
    bool late=rtl_station_fixture==RTL_STATION_USB_LATE;
    assert(rtl_station.failed && rtl_station.sends==1 && rtl_station.completed==(unsigned)late &&
           !rtl_station.auth_sent && !rtl_station.auth_ok);
    assert(trace->late.seen==late && trace->stop.seen!=late);
    if(late) {
        assert(trace->late.code==1 && trace->late.trb==trace->trb && !trace->late.residue);
        assert(trace->stop_end.dequeue==((trace->trb+16)|1));
    } else {
        assert(trace->stop.code==26 && trace->stop.trb==trace->trb && trace->stop.residue==trace->length);
        assert(trace->stop_end.dequeue==(trace->trb|1));
    }
    if(rtl_station_fixture<=RTL_STATION_USB_LATE) {
        bool spin=rtl_station_fixture==RTL_STATION_USB_SPIN_LIMIT;
        assert(!strcmp(trace->outcome,spin?"spin-limit":"deadline") && !trace->wait.seen);
        assert((trace->elapsed_us<RTL_CU_STATION_TX_US)==spin);
    } else {
        assert(!strcmp(trace->outcome,"event-rejected") && trace->wait.seen);
        assert(trace->wait.code==(bad_code?4:1));
        assert(trace->wait.trb==trace->trb+(rtl_station_fixture==RTL_STATION_USB_WRONG_TRB?16:0));
        assert(trace->wait.residue==(rtl_station_fixture==RTL_STATION_USB_RESIDUE?7:0));
        assert(trace->wait.event_data==(rtl_station_fixture==RTL_STATION_USB_EVENT_DATA));
    }
}
