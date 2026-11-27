/* Explicit laboratory station lifetime for the Demers2.4 reference AP. */
#define RTL_CU_STATION_CR (RTL_CU_LISTEN_CR|RTL8188CU_TX_ENABLE)
#define RTL_CU_STATION_RX_US 1500000u
#define RTL_CU_STATION_TX_US 250000u
#define RTL_CU_STATION_GENERATION 1u

static void rtl_cu_erase(void *memory,size_t size) {
    volatile uint8_t *out=memory;
    while(size--)*out++=0;
}

static bool rtl_cu_station_same_hub(const struct xhci_device *d) {
    unsigned siblings=0;
    for(unsigned i=0;i<XHCI_MAX_DEVICES;i++) {
        struct xhci_device *peer=&xhci.devices[i];
        if(!peer->present || device_removed(peer) || peer->parent_slot_id!=d->parent_slot_id)continue;
        if(peer->hub_port_id==2 && peer->vendor_id==0x7392 && peer->product_id==0xa711)siblings|=1;
        if(peer->hub_port_id==4 && peer->vendor_id==0x0846 && peer->product_id==0x9055)siblings|=2;
    }
    return siblings==3;
}

static struct rtl_cu_out_context rtl_cu_station_out_context(const struct xhci_device *d) {
    if(!d->output_context)return (struct rtl_cu_out_context){0};
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return (struct rtl_cu_out_context){
        .dequeue=context_read32(d->output_context,RTL_CU_TX_DCI,2)|
            ((uint64_t)context_read32(d->output_context,RTL_CU_TX_DCI,3)<<32),
        .state=context_read32(d->output_context,RTL_CU_TX_DCI,0)&7,
        .sampled=true,
    };
}

static bool rtl_cu_station_out_ready(struct xhci_device *d) {
    const struct usb_setup_packet setup={.request_type=0x82,.request=USB_REQ_GET_STATUS,
        .index=RTL_CU_TX_DCI/2,.length=2};
    int actual;
    rtl_station.out_gate=rtl_cu_station_out_context(d);
    if(!rtl_station.out_gate.sampled)return false;
    rtl_station.out_info=context_read32(d->output_context,RTL_CU_TX_DCI,1);
    rtl_station.out_status_valid=control_transfer_internal(d,&setup,d->descriptor_buffer_phys,2,true,&actual) &&
        actual==2;
    if(rtl_station.out_status_valid)
        rtl_station.out_status=d->descriptor_buffer[0]|(uint16_t)d->descriptor_buffer[1]<<8;
    return rtl_station.out_status_valid && rtl_station.out_status==0 && rtl_station.out_gate.state==1 &&
        rtl_station.out_gate.dequeue==(rtl_station.ep.ring_phys|1) &&
        (rtl_station.out_info>>16)==512 && ((rtl_station.out_info>>3)&7)==2;
}

static bool rtl_cu_write_address(struct xhci_device *d,uint16_t reg,const uint8_t *id) {
    return rtl_cu_write_checked(d,reg,4,rtl_cu_le32(id)) &&
        rtl_cu_write_checked(d,reg+4,2,id[4]|(uint32_t)id[5]<<8);
}

static bool rtl_cu_station_prepare(struct xhci_device *d,struct xhci_device *hub) {
    if(!rtl_station.peer.rsne_size || !rtl_station.queue_ready || !rtl_cu_station_same_hub(d) ||
       !rtl8188cu_check_port(d,hub) || !rtl_cu_mac_held(d,RTL_CU_LISTEN_CR))return false;
    static const struct {uint16_t reg,width;uint32_t value;} setup[]={
        {0x42a,2,0x3030},{0x430,4,0x01000000},{0x434,4,0x07060504},
        {0x438,4,0x01000000},{0x43c,4,0x07060504},{0x440,2,0x015d},{0x640,1,0x40},
        {0x500,4,0xff02},{0x504,4,0xff02},{0x508,4,0xff02},{0x50c,4,0xff02},
        /* Channel 10 transmit gain from the same adapter's Linux run. */
        {0xe08,4,0x0390282a},{0x86c,4,0x28282800},{0xe00,4,0x2a2a2a2a},
        {0xe04,4,0x2a2a2a2a},{0xe10,4,0x28282828},{0xe14,4,0x28282828},
        {0xe18,4,0x28282828},{0xe1c,4,0x28282828},{0xc90,1,0x20},{0xc91,1,0x18},{0xc92,1,0x12},
        {0x102,1,2},{0x550,1,0x18},{0x680,1,0},{0x608,4,0xf000690e},
    };
    for(unsigned i=0;i<sizeof setup/sizeof setup[0];i++)
        if(!rtl_cu_write_checked(d,setup[i].reg,setup[i].width,setup[i].value))return false;
    if(!rtl_cu_write_address(d,0x610,rtl_station.peer.station) ||
       !rtl_cu_write_address(d,0x618,rtl_station.peer.bssid) ||
       !rtl8188cu_check_port(d,hub) || !rtl_cu_tx_sample(d,&rtl_station.auth_queue))return false;
    const uint32_t *v=rtl_station.auth_queue.value;
    /* The same adapter's Linux SIE map assigns HQ to OUT 2 and NQ to OUT 3. */
    if(!rtl_cu_tx_allocation_matches(&rtl_station.auth_queue) ||
       v[RTL_CU_TX_SIE_HIGH_NORMAL_SAMPLE]!=0x32 || (v[RTL_CU_TX_SIE_LOW_SAMPLE]&0xf) ||
       v[RTL_CU_TX_MAP_SAMPLE]!=RTL_CU_TX_QUEUE_MAP ||
       v[RTL_CU_TX_CR_SAMPLE]!=RTL_CU_LISTEN_CR || v[RTL_CU_TX_PAUSE_SAMPLE]!=0xff ||
       !rtl_cu_station_out_ready(d) || !rtl_cu_mac_held(d,RTL_CU_LISTEN_CR) ||
       !rtl_cu_write_checked(d,RTL8188CU_MAC_CONTROL,2,RTL_CU_STATION_CR))return false;
    rtl_station.enabled=rtl_cu_write_checked(d,RTL_CU_TX_PAUSE,1,0);
    return rtl_station.enabled;
}

static bool rtl_cu_station_event(const struct xhci_device *d,uint8_t dci,uint64_t pointer,
                                  uint8_t completion,uint32_t remaining,bool event_data) {
    if(!rtl_station.requested || !d || d!=rtl_passive.owner || dci!=RTL_CU_TX_DCI ||
       !rtl_passive.configured)return false;
    bool stop_event=rtl_station.stopping && completion>=26 && completion<=28;
    struct rtl_cu_auth_trace *trace=&rtl_station.auth_out;
    if(trace->trb && rtl_station.sends==1) {
        struct rtl_cu_out_event *event=stop_event?&trace->stop:trace->outcome?&trace->late:&trace->wait;
        if(!event->seen)*event=(struct rtl_cu_out_event){
            .trb=pointer,.residue=remaining,.code=completion,.seen=true,.event_data=event_data,
        };
    }
    if(stop_event && !event_data && !(pointer&15) && pointer>=rtl_station.ep.ring_phys &&
       pointer-rtl_station.ep.ring_phys<XHCI_TRB_COUNT*sizeof(struct xhci_trb))return true;
    if(!rtl_station.pending || pointer!=rtl_station.trb || event_data || completion!=1 || remaining) {
        rtl_station.failed=true;
        return true;
    }
    rtl_station.pending=false;
    rtl_station.completed++;
    return true;
}

static bool rtl_cu_station_owned(struct xhci_device *d) {
    return rtl_station.enabled && rtl_passive.owner==d && rtl_passive.configured &&
        !rtl_station.failed && !rtl_passive.failed && !xhci.stopping &&
        !d->control_failed && !d->ep0_quarantined && !device_removed(d) &&
        rtl_cu_station_same_hub(d);
}

static void rtl_cu_auth_wait_end(struct xhci_device *d,struct rtl_cu_auth_trace *trace,
                                 uint64_t start,const char *outcome) {
    trace->wait_end=rtl_cu_station_out_context(d);
    trace->elapsed_us=timer_monotonic_us()-start;
    trace->outcome=outcome;
}

static bool rtl_cu_station_send(struct xhci_device *d,enum rtl_cu_station_tx type,
                                 const uint8_t *key,size_t size) {
    struct xhci_bulk_endpoint *ep=&rtl_station.ep;
    if(!rtl_cu_station_owned(d) || rtl_station.pending || rtl_station.sends!=rtl_station.completed ||
       rtl_station.sends>=5)return false;
    size_t length=rtl_cu_station_tx(&rtl_station.peer,type,rtl_station.sends+1,rtl_station.qos,
        key,size,rtl_station.buffer,RTL_CU_STATION_TX_MAX);
    if(!length)return false;
    struct xhci_trb *entry=&ep->ring[ep->enqueue];
    struct xhci_trb transfer={.parameter_low=(uint32_t)rtl_station.physical,
        .parameter_high=rtl_station.physical>>32,.status=(uint32_t)length,
        .control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC};
    rtl_station.trb=ring_push_owned(ep->ring,ep->ring_phys,&ep->enqueue,&ep->cycle,&transfer,true);
    rtl_station.pending=true;
    rtl_station.sends++;
    bool first_auth=type==RTL_CU_AUTH && rtl_station.sends==1;
    struct rtl_cu_auth_trace *trace=&rtl_station.auth_out;
    if(first_auth) {
        trace->trb=rtl_station.trb;
        trace->slot=d->slot_id;
        trace->length=length;
        trace->before=rtl_cu_station_out_context(d);
    }
    __atomic_thread_fence(__ATOMIC_RELEASE);
    ((volatile struct xhci_trb *)entry)->control=transfer.control|1;
    db_write32(d->slot_id,RTL_CU_TX_DCI);
    uint64_t start=timer_monotonic_us();
    const char *outcome="spin-limit";
    for(uint64_t spins=0;spins<XHCI_TRANSFER_TIMEOUT;spins++) {
        struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
        if(!rtl_cu_station_owned(d)) {
            outcome=rtl_station.failed?"event-rejected":"ownership-lost";
            break;
        }
        if(!rtl_station.pending) {
            if(first_auth)rtl_cu_auth_wait_end(d,trace,start,"complete");
            rtl_cu_erase(rtl_station.buffer,RTL_CU_STATION_TX_MAX);
            return true;
        }
        if((spins&255)==0 && timer_monotonic_us()-start>=RTL_CU_STATION_TX_US) {
            outcome="deadline";
            break;
        }
        __asm__ volatile("pause");
    }
    if(first_auth)rtl_cu_auth_wait_end(d,trace,start,outcome);
    rtl_station.failed=true;
    return false;
}

static bool rtl_cu_station_rx(struct xhci_device *d,enum rtl_cu_station_tx wanted,uint8_t m1_digest[32]) {
    uint64_t start=timer_monotonic_us(),check=start;
    for(uint64_t spins=0;spins<XHCI_TRANSFER_TIMEOUT;spins++) {
        struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
        if(!rtl_cu_station_owned(d))return false;
        if(!rtl_passive.pending || (spins&255)==0) {
            uint64_t now=timer_monotonic_us();
            if(now-start>=RTL_CU_STATION_RX_US)return false;
            if(now-check>=100000) {
                uint32_t mac,pause;
                if(!rtl8188cu_read(d,RTL8188CU_MAC_CONTROL,2,&mac) || mac!=RTL_CU_STATION_CR ||
                   !rtl8188cu_read(d,RTL_CU_TX_PAUSE,1,&pause) || pause!=0) {
                    rtl_station.failed=true;
                    return false;
                }
                check=now;
            }
        }
        if(rtl_passive.pending) {
            __asm__ volatile("pause");
            continue;
        }
        const uint8_t *frame;
        size_t size;
        bool matched=false,accept=false;
        if(rtl_cu_rx_unwrap(rtl_passive.buffer,rtl_passive.actual,&frame,&size)==RTL_CU_RX_DISPOSITIONS) {
            if(wanted==RTL_CU_AUTH || wanted==RTL_CU_ASSOC) {
                struct rtl_cu_station_response response;
                enum rtl_cu_station_reply reply=rtl_cu_station_reply(&rtl_station.peer,wanted,frame,size,&response);
                matched=reply!=RTL_CU_REPLY_OTHER;
                accept=reply==RTL_CU_REPLY_OK;
                if(matched)rtl_station.reply_status=response.status;
                if(accept && wanted==RTL_CU_ASSOC) {
                    rtl_station.aid=response.aid;
                    rtl_station.qos=response.qos;
                }
            } else {
                const uint8_t *eapol;
                size_t length;
                if(rtl_cu_station_eapol(&rtl_station.peer,frame,size,&eapol,&length)) {
                    uint8_t digest[32];
                    sha256_digest(eapol,length,digest);
                    matched=wanted==RTL_CU_M2 || !bytes_equal(digest,m1_digest,sizeof digest);
                    if(matched) {
                        accept=wpa2_hs_receive(&rtl_station.handshake,RTL_CU_STATION_GENERATION,
                            rtl_station.peer.bssid,rtl_station.peer.station,eapol,length);
                        rtl_station.last_rx=rtl_station.handshake.last_rx;
                        if(accept && wanted==RTL_CU_M2)copy_bytes(m1_digest,digest,sizeof digest);
                    }
                    rtl_cu_erase(digest,sizeof digest);
                }
            }
        }
        rtl_cu_rx_inspect();
        if(!rtl_cu_rx_drain() || !rtl_cu_rx_post(d)) {
            rtl_station.failed=true;
            return false;
        }
        if(matched)return accept;
    }
    rtl_station.failed=true;
    return false;
}

static bool rtl_cu_station_send_key(struct xhci_device *d,enum rtl_cu_station_tx type) {
    uint8_t eapol[121];
    size_t size=wpa2_hs_emit(&rtl_station.handshake,RTL_CU_STATION_GENERATION,eapol,sizeof eapol);
    bool sent=size && rtl_cu_station_send(d,type,eapol,size);
    rtl_cu_erase(eapol,sizeof eapol);
    return size && wpa2_hs_tx_result(&rtl_station.handshake,RTL_CU_STATION_GENERATION,
        sent?WPA2_HS_CONFIRMED:rtl_station.failed?WPA2_HS_UNCERTAIN:WPA2_HS_REFUSED) && sent;
}

static bool rtl_cu_station_install_keys(void) {
    struct wpa2_hs_keys exported={0};
    bool installed=wpa2_hs_take_keys(&rtl_station.handshake,RTL_CU_STATION_GENERATION,&exported) &&
        wpa2_ccmp_install(&rtl_station.keys,&rtl_station.handshake,&exported);
    rtl_cu_erase(&exported,sizeof exported);
    return installed && wpa2_hs_install_result(&rtl_station.handshake,RTL_CU_STATION_GENERATION,WPA2_HS_CONFIRMED);
}

static bool rtl_cu_station_exchange(struct xhci_device *d,struct xhci_device *hub,const uint8_t *pmk) {
    struct wpa2_hs_peer peer={.rsne_size=rtl_station.peer.rsne_size};
    copy_bytes(peer.aa,rtl_station.peer.bssid,sizeof peer.aa);
    copy_bytes(peer.spa,rtl_station.peer.station,sizeof peer.spa);
    copy_bytes(peer.rsne,rtl_station.peer.rsne,peer.rsne_size);
    uint8_t nonce[32]={0},m1_digest[32]={0};
    rtl_station.phase="peer";
#ifdef SRVROS_RTL8188CU_STATION_TEST
    extern bool rtl_cu_station_test_nonce(uint8_t out[32]);
    bool fresh=rtl_cu_station_test_nonce(nonce);
#else
    bool fresh=wpa2_nonce_generate(nonce);
#endif
    bool ok=rtl_station.peer.rsne_size && fresh &&
        wpa2_hs_start(&rtl_station.handshake,&peer,pmk,nonce,RTL_CU_STATION_GENERATION);
    rtl_cu_erase(nonce,sizeof nonce);
    if(ok) {
        rtl_station.phase="tx-setup";
        rtl_station.ready=rtl_cu_station_prepare(d,hub);
        ok=rtl_station.ready;
    }
    if(ok) {
        rtl_station.phase="auth";
        rtl_station.auth_sent=rtl_cu_station_send(d,RTL_CU_AUTH,NULL,0);
        rtl_station.auth_ok=rtl_station.auth_sent && rtl_cu_station_rx(d,RTL_CU_AUTH,m1_digest);
        ok=rtl_station.auth_ok;
    }
    if(ok) {
        rtl_station.phase="assoc";
        rtl_station.assoc_sent=rtl_cu_station_send(d,RTL_CU_ASSOC,NULL,0);
        rtl_station.assoc_ok=rtl_station.assoc_sent && rtl_cu_station_rx(d,RTL_CU_ASSOC,m1_digest);
        ok=rtl_station.assoc_ok;
    }
    if(ok) {
        rtl_station.phase="m1";
        rtl_station.m1=rtl_cu_station_rx(d,RTL_CU_M2,m1_digest);
        ok=rtl_station.m1;
    }
    if(ok) {
        rtl_station.phase="m2";
        rtl_station.m2=rtl_cu_station_send_key(d,RTL_CU_M2);
        ok=rtl_station.m2;
    }
    if(ok) {
        rtl_station.phase="m3";
        rtl_station.m3=rtl_cu_station_rx(d,RTL_CU_M4,m1_digest);
        ok=rtl_station.m3 && rtl_cu_station_install_keys();
    }
    if(ok) {
        rtl_station.phase="m4";
        rtl_station.m4=rtl_cu_station_send_key(d,RTL_CU_M4);
        ok=rtl_station.m4 && wpa2_ccmp_activate(&rtl_station.keys,&rtl_station.handshake);
    }
    rtl_cu_erase(m1_digest,sizeof m1_digest);
    if(rtl_station.keys.phase!=WPA2_CCMP_EMPTY)
        rtl_station.keys_erased=wpa2_ccmp_retire(&rtl_station.keys,RTL_CU_STATION_GENERATION) &&
            wpa2_ccmp_erased(&rtl_station.keys,RTL_CU_STATION_GENERATION);
    else rtl_station.keys_erased=true;
    if(rtl_station.handshake.phase!=WPA2_HS_EMPTY)
        ok=wpa2_hs_retire(&rtl_station.handshake,RTL_CU_STATION_GENERATION,rtl_station.keys_erased) && ok;
    if(rtl_station.auth_ok && rtl_cu_station_owned(d) && timer_delay_us(25000)) {
        rtl_station.leave=rtl_cu_station_send(d,RTL_CU_LEAVE,NULL,0) && timer_delay_us(25000);
    }
    if(ok && rtl_station.leave)rtl_station.phase="complete";
    return ok && rtl_station.keys_erased && rtl_station.leave;
}

static bool rtl_cu_station_stop(struct xhci_device *d,struct xhci_device *hub) {
    /* Close new radio publication before the USB OUT ownership proof. */
    bool paused=rtl8188cu_check_port(d,hub) && rtl_cu_write_checked(d,RTL_CU_TX_PAUSE,1,0xff);
    rtl_station.enabled=false;
    uint64_t slot=UINT64_MAX;
    rtl_station.stopping=true;
    bool stopped=submit_command(XHCI_TRB_TYPE_STOP_ENDPOINT,((uint32_t)d->slot_id<<24)|(RTL_CU_TX_DCI<<16),&slot);
    rtl_station.stopping=false;
    struct rtl_cu_out_context context=rtl_cu_station_out_context(d);
    if(rtl_station.auth_out.trb)rtl_station.auth_out.stop_end=context;
    if(slot!=UINT64_MAX && slot!=d->slot_id)xhci.command_failed=true;
    if(!stopped || slot!=d->slot_id)return false;
    uint64_t at=rtl_station.pending?rtl_station.trb:
        rtl_station.ep.ring_phys+rtl_station.ep.enqueue*sizeof(struct xhci_trb);
    rtl_station.tx_stopped=context.sampled && context.state==3 && context.dequeue==(at|1);
    if(rtl_station.tx_stopped) {
        rtl_station.pending=false;
        rtl_cu_erase(rtl_station.buffer,RTL_CU_STATION_TX_MAX);
    }
    return paused && rtl_station.tx_stopped;
}

static void rtl_cu_auth_log_context(const char *stage,const struct rtl_cu_out_context *context) {
    console_printf("rtl8188cu-auth-context: stage=%s sampled=%u state=%u dequeue=%x\n",
        stage,(uint64_t)context->sampled,(uint64_t)context->state,context->dequeue);
}

static void rtl_cu_auth_log_event(const char *stage,const struct rtl_cu_out_event *event) {
    console_printf("rtl8188cu-auth-event: stage=%s seen=%u code=%u residue=%u trb=%x ed=%u\n",
        stage,(uint64_t)event->seen,(uint64_t)event->code,(uint64_t)event->residue,event->trb,
        (uint64_t)event->event_data);
}

static void rtl_cu_tx_log_sample(const char *stage,const struct rtl_cu_tx_sample *sample) {
    const uint32_t *v=sample->value;
    console_printf("rtl8188cu-tx-queue: stage=%s reads=%u rqpn=%x npq=%x map=%x mgq=%x sie_hn=%x sie_low=%x cr=%x pause=%x\n",
        stage,(uint64_t)sample->reads,(uint64_t)v[RTL_CU_TX_PAGES_SAMPLE],(uint64_t)v[RTL_CU_TX_NORMAL_SAMPLE],
        (uint64_t)v[RTL_CU_TX_MAP_SAMPLE],(uint64_t)v[RTL_CU_TX_BOUNDARY_SAMPLE],
        (uint64_t)v[RTL_CU_TX_SIE_HIGH_NORMAL_SAMPLE],(uint64_t)v[RTL_CU_TX_SIE_LOW_SAMPLE],
        (uint64_t)v[RTL_CU_TX_CR_SAMPLE],(uint64_t)v[RTL_CU_TX_PAUSE_SAMPLE]);
}

static void rtl_cu_station_log(bool complete) {
    console_printf("rtl8188cu-wpa2: phase=%s ready=%u auth_tx=%u auth=%u assoc_tx=%u assoc=%u status=%u aid=%u m1=%u m2=%u m3_mic=%u m4_usb=%u leave=%u keys_erased=%u tx_stopped=%u sends=%u completed=%u last_rx=%u unsafe=%u complete=%u\n",
        rtl_station.phase?rtl_station.phase:"admission",(uint64_t)rtl_station.ready,
        (uint64_t)rtl_station.auth_sent,(uint64_t)rtl_station.auth_ok,(uint64_t)rtl_station.assoc_sent,
        (uint64_t)rtl_station.assoc_ok,(uint64_t)rtl_station.reply_status,(uint64_t)rtl_station.aid,
        (uint64_t)rtl_station.m1,(uint64_t)rtl_station.m2,(uint64_t)rtl_station.m3,(uint64_t)rtl_station.m4,
        (uint64_t)rtl_station.leave,(uint64_t)rtl_station.keys_erased,(uint64_t)rtl_station.tx_stopped,
        (uint64_t)rtl_station.sends,(uint64_t)rtl_station.completed,(uint64_t)rtl_station.last_rx,
        (uint64_t)rtl_station.failed,(uint64_t)complete);
    if(rtl_station.queue_attempted) {
        rtl_cu_tx_log_sample("pre-firmware",&rtl_station.early_queue);
        rtl_cu_tx_log_sample("pre-auth",&rtl_station.auth_queue);
        const struct rtl_cu_out_context *out=&rtl_station.out_gate;
        console_printf("rtl8188cu-tx-readiness: allocated=%u load=%x sampled=%u state=%u dequeue=%x info=%x status_valid=%u status=%x ready=%u\n",
            (uint64_t)rtl_station.queue_ready,(uint64_t)rtl_station.queue_load,(uint64_t)out->sampled,
            (uint64_t)out->state,out->dequeue,(uint64_t)rtl_station.out_info,
            (uint64_t)rtl_station.out_status_valid,(uint64_t)rtl_station.out_status,(uint64_t)rtl_station.ready);
    }
    const struct rtl_cu_auth_trace *trace=&rtl_station.auth_out;
    if(!trace->trb)return;
    console_printf("rtl8188cu-auth-out: outcome=%s slot=%u dci=%u trb=%x length=%u elapsed_us=%u\n",
        trace->outcome,(uint64_t)trace->slot,(uint64_t)RTL_CU_TX_DCI,trace->trb,
        (uint64_t)trace->length,trace->elapsed_us);
    rtl_cu_auth_log_context("before",&trace->before);
    rtl_cu_auth_log_context("wait-end",&trace->wait_end);
    rtl_cu_auth_log_context("stop-end",&trace->stop_end);
    rtl_cu_auth_log_event("wait",&trace->wait);
    rtl_cu_auth_log_event("late",&trace->late);
    rtl_cu_auth_log_event("stop",&trace->stop);
}
