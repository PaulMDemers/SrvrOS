#include "fixtures/wpa2-handshake-synthetic.h"

enum rtl_station_fixture {
    RTL_STATION_OFF,RTL_STATION_OK,RTL_STATION_BAD_MIC,RTL_STATION_USB_TIMEOUT,RTL_STATION_USB_SPIN_LIMIT,
    RTL_STATION_USB_LATE,RTL_STATION_USB_BAD_CODE,RTL_STATION_USB_RESIDUE,RTL_STATION_USB_WRONG_TRB,
    RTL_STATION_USB_EVENT_DATA,RTL_STATION_NO_SIBLING,
};
static enum rtl_station_fixture rtl_station_fixture;
static bool rtl_station_test_m1;

static void rtl_radio_out_event(unsigned offset,uint64_t trb,unsigned code,unsigned residue,bool event_data) {
    rtl_radio_transfer_event(offset,trb,code,residue);
    unsigned index=(xhci.event_dequeue+offset)%XHCI_EVENT_TRB_COUNT;
    xhci.event_ring[index].control^=(3u^RTL_CU_TX_DCI)<<16;
    if(event_data)xhci.event_ring[index].control|=4;
}

bool rtl_cu_station_test_nonce(uint8_t out[32]) {
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
    rtl_radio_model.rx_index++;
}

static bool rtl_radio_station_doorbell(uint64_t value) {
    static const uint8_t frame_control[]={0xb0,0,0x88,0x88,0xc0};
    if(value==RTL_CU_TX_DCI) {
        assert(rtl_station_fixture!=RTL_STATION_OFF && rtl_station.pending);
        assert(rtl_radio_cr()==0x02ff && !rtl_radio_model.reg[0x522]);
        const struct xhci_trb *trb=(const struct xhci_trb *)(uintptr_t)rtl_station.trb;
        assert(((uint64_t)trb->parameter_high<<32|trb->parameter_low)==rtl_station.physical);
        unsigned sent=rtl_station.sends;
        assert(sent>0 && sent<=5);
        const uint8_t *packet=rtl_station.buffer;
        assert(trb->status==(unsigned)(packet[0]|packet[1]<<8)+32 && packet[2]==32 && packet[3]==0x8c);
        unsigned check=0;
        for(unsigned i=0;i<32;i+=2)check^=packet[i]|(unsigned)packet[i+1]<<8;
        assert(!check && packet[14]==sent && packet[17]==1);
        assert(!memcmp(packet+36,hs_aa,6) && !memcmp(packet+42,hs_spa,6) && !memcmp(packet+48,hs_aa,6));
        bool leaving=packet[32]==0xc0;
        if(leaving)assert(sent==5 || (sent==4 && rtl_station_fixture==RTL_STATION_BAD_MIC));
        else assert(packet[32]==frame_control[sent-1]);
        assert(packet[5]==((!leaving && (sent==3 || sent==4))?7:0x12));
        if(!leaving && sent==3)assert(!memcmp(packet+66,hs_m2,sizeof hs_m2));
        if(!leaving && sent==4)assert(!memcmp(packet+66,hs_m4,sizeof hs_m4));
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
        if(sent==3 && !leaving)rtl_radio_station_response(1,0x88,hs_m3,sizeof hs_m3);
        return true;
    }
    if(value==RTL_CU_RX_DCI && rtl_station.enabled) {
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
        assert(!rtl_station.ready && !rtl_station.sends && !rtl_station.enabled && !trace->trb);
        return;
    }
    assert(trace->trb==rtl_station.ep.ring_phys && trace->slot==2 && trace->length==62 && trace->elapsed_us);
    assert(trace->before.sampled && trace->before.state==1 && trace->before.dequeue==(trace->trb|1));
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
