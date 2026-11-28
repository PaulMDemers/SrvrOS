/* One-shot Demers2.4 station exchange. This path never opens an IP port. */
enum {
    BU_STA_MAC=0x610, BU_STA_BSSID=0x618,
    BU_STA_GRANTS=0xff00, BU_STA_WLAN_GRANT=0x7700, BU_STA_RX_US=300000,
    BU_STA_GENERATION=1,
};

static bool bu_station_configure(struct bu_upload *up) {
    struct xhci_device *d=up->device;
    /* Linux has both OUT pipes before its first transfer. Keep the slot's
     * endpoint set stable from firmware upload through the first Auth. */
    up->eapol_ready=bu_configure_out_pipe(d,&up->eapol,BU_EAPOL_DCI);
    console_printf("rtl8822bu-station-pipes: phase=pre-firmware mgmt=%u eapol=%u configured=%u entries=%u\n",
        (uint64_t)up->out.dci,(uint64_t)BU_EAPOL_DCI,(uint64_t)up->eapol_ready,
        (uint64_t)(context_read32(d->output_context,0,0)>>27));
    return up->eapol_ready;
}

static bool bu_station_prepare(struct bu_upload *up,struct bu_rx *rx) {
    struct xhci_device *d=up->device;
    bool identity=bytes_equal(up->station,rx->station->peer.station,6);
    bool ready=identity && wifi_hub_check_port(d,up->hub) &&
        bu_rx_cr_verified(d,BU_RX_DMA_CR) && !d->bulk_pending;
    bool verified=ready && up->eapol_ready &&
        (context_read32(d->output_context,0,0)>>27)>=BU_EAPOL_DCI &&
        (context_read32(d->output_context,BU_OUT_DCI,0)&7)==1 &&
        context_read32(d->output_context,BU_OUT_DCI,1)==0x02000016 &&
        (context_read32(d->output_context,BU_EAPOL_DCI,0)&7)==1 &&
        context_read32(d->output_context,BU_EAPOL_DCI,1)==0x02000016 &&
        bu_pipe_dequeue(d,BU_EAPOL_DCI)==(up->eapol.ring_phys|1);
    console_printf("rtl8822bu-station-prepare: bound=%u admitted=%u configured=%u verified=%u\n",
        (uint64_t)identity,(uint64_t)ready,(uint64_t)up->eapol_ready,(uint64_t)verified);
    return verified;
}

static bool bu_station_address(struct xhci_device *d,unsigned reg,const uint8_t *address) {
    uint32_t low=rtl_cu_le32(address),high=address[4]|(uint32_t)address[5]<<8;
    uint32_t a,b;
    return bu_write(d,reg,4,low) && bu_write(d,reg+4,2,high) &&
        bu_read(d,reg,4,&a) && bu_read(d,reg+4,2,&b) && a==low && b==high;
}

static bool bu_station_radio(struct bu_upload *up,struct bu_rx *rx) {
    struct xhci_device *d=up->device;
    uint32_t grant,pause;
    if(!wifi_hub_check_port(d,up->hub) || !bu_rx_scan_guard(d) ||
       !bu_station_address(d,BU_STA_MAC,rx->station->peer.station) ||
       !bu_station_address(d,BU_STA_BSSID,rx->station->peer.bssid) ||
       !bu_rx_rf_grant_read(d,&grant))return false;
    /* Snapshot the three fields left untouched by the passive inhibit. */
    rx->rf_grant_saved=(grant&~BU_RX_RF_GRANT_MASK)|(rx->rf_grant_saved&BU_RX_RF_GRANT_MASK);
    rx->rf_grant_mask=BU_STA_GRANTS;
    rx->rf_grant_touched=true;
    return bu_rx_rf_grant_write(d,(grant&~BU_STA_GRANTS)|BU_STA_WLAN_GRANT) &&
        bu_write(d,BU_TXPAUSE,1,0) && bu_read(d,BU_TXPAUSE,1,&pause) && !pause;
}

/* Read only after the first management OUT times out, before disabling TX.
 * EP0 may dispatch a late OUT event. Preserve the original failure. */
static void bu_station_trace_auth(struct bu_upload *up) {
    struct xhci_device *d=up->device;
    uint32_t cr=0,pause=0,dma=0,empty=0,high=0,public_pages=0;
    unsigned valid=0;
    if(bu_read(d,BU_CR,2,&cr))valid|=1;
    if(bu_read(d,BU_TXPAUSE,1,&pause))valid|=2;
    if(bu_read(d,0x210,4,&dma))valid|=4; /* TXDMA_STATUS */
    if(bu_read(d,0x41a,2,&empty))valid|=8; /* TXPKT_EMPTY */
    if(bu_read(d,0x230,4,&high))valid|=16;
    if(bu_read(d,0x240,4,&public_pages))valid|=32;
    console_printf("rtl8822bu-auth-device: valid=%x cr=%x pause=%x dma=%x empty=%x hpq=%x pubq=%x pending=%u cc=%u\n",
        (uint64_t)valid,(uint64_t)cr,(uint64_t)pause,(uint64_t)dma,(uint64_t)empty,
        (uint64_t)high,(uint64_t)public_pages,(uint64_t)d->bulk_pending,(uint64_t)d->bulk_completion);
}

static bool bu_station_send(struct bu_upload *up,struct bu_rx *rx,enum rtl_cu_station_tx type,
                            const uint8_t *body,size_t size) {
    struct xhci_device *d=up->device;
    struct bu_station *sta=rx->station;
    if(rx->failed || d->bulk_pending || !wifi_hub_check_port(d,up->hub))return false;
    uint8_t frame[RTL_CU_STATION_TX_MAX];
    size_t length=rtl_cu_station_tx(&sta->peer,type,++sta->sequence,sta->reply.qos,
        body,size,frame,sizeof frame);
    if(length<=32)return false;
    length-=32;
    bool key=type==RTL_CU_M2 || type==RTL_CU_M4;
    struct xhci_bulk_endpoint *ep=key?&up->eapol:&up->out;
    uint8_t *tx=up->packet;
    zero_bytes(tx,BU_DESC_BYTES+length);
    tx[0]=(uint8_t)length;
    tx[1]=(uint8_t)(length>>8);
    tx[2]=BU_DESC_BYTES;
    tx[3]=key?0x04:0x84; /* LS, and disable queue sequence on management. */
    tx[5]=key?7:0x12;
    tx[6]=8; /* 2.4 GHz legacy 1 Mbps rate table. */
    tx[13]=5; /* Use a fixed rate and disable fallback. */
    if(key) {
        tx[36]=(uint8_t)(sta->sequence<<4);
        tx[37]=(uint8_t)(sta->sequence>>4);
    } else tx[33]=0x80; /* Management uses hardware sequence on port zero. */
    uint16_t checksum=0;
    for(unsigned i=0;i<32;i+=2)checksum^=tx[i]|(uint16_t)tx[i+1]<<8;
    tx[28]=(uint8_t)checksum;tx[29]=(uint8_t)(checksum>>8);
    copy_bytes(tx+BU_DESC_BYTES,frame+32,length);
    rtl_cu_erase(frame,sizeof frame);
    bool sent=bu_post_out(up,ep,(unsigned)(length+BU_DESC_BYTES),true,"station");
    if(!sent && type==RTL_CU_AUTH && d->bulk_pending) {
        up->trace_auth_stop=true;
        bu_station_trace_auth(up);
    }
    if(!d->bulk_pending)rtl_cu_erase(tx,BU_DESC_BYTES+length);
    console_printf("rtl8822bu-station-tx: frame=%u dci=%u usb=%u cc=%u residue=%u pending=%u\n",
        (uint64_t)type,(uint64_t)ep->dci,(uint64_t)sent,(uint64_t)d->bulk_completion,
        (uint64_t)d->bulk_remaining,(uint64_t)d->bulk_pending);
    return sent;
}

/* The current completion stays owned while a returned MPDU is in use. */
static bool bu_station_next_frame(struct bu_rx *rx,const uint8_t **mpdu,size_t *size) {
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    while(rx->consumed<rx->completed) {
        unsigned slot=rx->consumed%BU_RX_DEPTH;
        const uint8_t *data=rx->buffer+slot*BU_RX_BYTES;
        size_t actual=rx->actual[slot];
        while(rx->frame_offset<=actual && actual-rx->frame_offset>=24) {
            const uint8_t *row=data+rx->frame_offset;
            uint32_t desc=rtl_cu_le32(row),meta=rtl_cu_le32(row+8);
            size_t prefix=24+8*((desc>>16)&15)+((desc>>24)&3),length=desc&0x3fff;
            if(!length || prefix+length>actual-rx->frame_offset)break;
            rx->frame_offset+=(prefix+length+7u)&~7u;
            if((meta&(1u<<28)) || (desc&0xc000) || length<=4)continue;
            *mpdu=row+prefix;
            *size=length-4;
            return true;
        }
        rx->frame_offset=0;
        rx->consumed++;
        bu_rx_fill(rx);
    }
    return false;
}

static bool bu_station_wait(struct bu_rx *rx,enum rtl_cu_station_tx wanted) {
    struct bu_station *sta=rx->station;
    uint64_t start=timer_monotonic_us();
    for(unsigned spin=0;spin<XHCI_TRANSFER_TIMEOUT && !rx->failed;spin++) {
        struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
        if(rx->failed || rx->posted==rx->consumed || device_removed(rx->device))return false;
        const uint8_t *frame;
        size_t size;
        while(bu_station_next_frame(rx,&frame,&size)) {
            if(wanted==RTL_CU_AUTH || wanted==RTL_CU_ASSOC) {
                enum rtl_cu_station_reply match=rtl_cu_station_reply(&sta->peer,wanted,frame,size,&sta->reply);
                if(match!=RTL_CU_REPLY_OTHER)return match==RTL_CU_REPLY_OK;
            } else {
                const uint8_t *eapol;
                size_t length;
                if(!rtl_cu_station_eapol(&sta->peer,frame,size,&eapol,&length))continue;
                /* Skip a retransmitted M1 after our M2. */
                if(wanted==RTL_CU_M4 && !(eapol[5]&1))continue;
                return wpa2_hs_receive(&sta->handshake,BU_STA_GENERATION,sta->peer.bssid,
                    sta->peer.station,eapol,length);
            }
        }
        if((spin&255u)==0 && timer_monotonic_us()-start>=BU_STA_RX_US)break;
        __asm__ volatile("pause");
    }
    return false;
}

static bool bu_station_key_tx(struct bu_upload *up,struct bu_rx *rx,enum rtl_cu_station_tx type) {
    struct wpa2_handshake *hs=&rx->station->handshake;
    uint8_t key[121];
    size_t length=wpa2_hs_emit(hs,BU_STA_GENERATION,key,sizeof key);
    bool sent=length && bu_station_send(up,rx,type,key,length);
    rtl_cu_erase(key,sizeof key);
    return length && wpa2_hs_tx_result(hs,BU_STA_GENERATION,sent?WPA2_HS_CONFIRMED:
        up->device->bulk_pending?WPA2_HS_UNCERTAIN:WPA2_HS_REFUSED) && sent;
}

static bool bu_station_install_keys(struct bu_station *sta) {
    struct wpa2_hs_keys exported={0};
    bool ready=wpa2_hs_take_keys(&sta->handshake,BU_STA_GENERATION,&exported) &&
        wpa2_ccmp_install(&sta->keys,&sta->handshake,&exported);
    rtl_cu_erase(&exported,sizeof exported);
    return ready && wpa2_hs_install_result(&sta->handshake,BU_STA_GENERATION,WPA2_HS_CONFIRMED);
}

static const char *bu_station_exchange(struct bu_upload *up,struct bu_rx *rx) {
    struct bu_station *sta=rx->station;
    struct wpa2_hs_peer binding={.rsne_size=sta->peer.rsne_size};
    copy_bytes(binding.aa,sta->peer.bssid,6);
    copy_bytes(binding.spa,sta->peer.station,6);
    copy_bytes(binding.rsne,sta->peer.rsne,binding.rsne_size);
    uint8_t nonce[32]={0};
#ifdef SRVROS_RTL8822BU_STATION_TEST
    extern bool bu_test_station_nonce(uint8_t out[32]);
    bool fresh=bu_test_station_nonce(nonce);
#else
    bool fresh=wpa2_nonce_generate(nonce);
#endif
    fresh=fresh && wpa2_hs_start(&sta->handshake,&binding,sta->pmk,nonce,BU_STA_GENERATION);
    rtl_cu_erase(nonce,sizeof nonce);
    if(fresh && bu_station_radio(up,rx)) {
        sta->auth=bu_station_send(up,rx,RTL_CU_AUTH,NULL,0) && bu_station_wait(rx,RTL_CU_AUTH);
        sta->assoc=sta->auth && bu_station_send(up,rx,RTL_CU_ASSOC,NULL,0) && bu_station_wait(rx,RTL_CU_ASSOC);
        sta->m1=sta->assoc && bu_update(up->device,BU_CR+2,1,3,2) && bu_station_wait(rx,RTL_CU_M2);
        sta->m2=sta->m1 && bu_station_key_tx(up,rx,RTL_CU_M2);
        sta->m3=sta->m2 && bu_station_wait(rx,RTL_CU_M4);
        sta->m4=sta->m3 && bu_station_install_keys(sta) && bu_station_key_tx(up,rx,RTL_CU_M4) &&
            wpa2_ccmp_activate(&sta->keys,&sta->handshake);
        if(sta->auth && !rx->failed && !up->device->bulk_pending && timer_delay_us(25000))
            sta->leave=bu_station_send(up,rx,RTL_CU_LEAVE,NULL,0) && timer_delay_us(25000);
    }
    sta->keys_erased=sta->keys.phase==WPA2_CCMP_EMPTY ||
        (wpa2_ccmp_retire(&sta->keys,BU_STA_GENERATION) && wpa2_ccmp_erased(&sta->keys,BU_STA_GENERATION));
    if(sta->handshake.phase!=WPA2_HS_EMPTY)
        sta->keys_erased=wpa2_hs_retire(&sta->handshake,BU_STA_GENERATION,sta->keys_erased) && sta->keys_erased;
    console_printf("rtl8822bu-wpa2: auth=%u assoc=%u aid=%u status=%u m1=%u m2=%u m3_mic=%u m4_usb=%u leave=%u erased=%u\n",
        (uint64_t)sta->auth,(uint64_t)sta->assoc,(uint64_t)sta->reply.aid,(uint64_t)sta->reply.status,
        (uint64_t)sta->m1,(uint64_t)sta->m2,(uint64_t)sta->m3,(uint64_t)sta->m4,
        (uint64_t)sta->leave,(uint64_t)sta->keys_erased);
    return sta->m4 && sta->leave && sta->keys_erased?NULL:"station-exchange";
}
