/* Ordered RFE type 3 table load with no MAC traffic. See tools/rtl8822bu_phy.py. */
enum {
    BU_PHY_ROWS=2891, BU_PHY_ROW_BYTES=7, BU_PHY_BYTES=BU_PHY_ROWS*BU_PHY_ROW_BYTES,
    BU_TXPAUSE=0x0522, BU_RXPSEL=0x0808, BU_RXPSEL_RESET=3u<<28,
};

static bool bu_phy_profile_valid(const uint8_t *profile,uint64_t length) {
    static const uint8_t expected[32]={
        0x85,0xc0,0xc8,0x6e,0xa0,0x19,0x58,0xa6,0x97,0xb7,0xc7,0xca,0x16,0x2f,0xe1,0xb9,
        0x5e,0x89,0x01,0x3b,0x67,0xde,0xf1,0x37,0x7b,0x5f,0xca,0xa9,0x44,0x36,0x22,0x4c,
    };
    uint8_t digest[32];
    if(!profile || length!=BU_PHY_BYTES)return false;
    sha256_digest(profile,length,digest);
    return bytes_equal(digest,expected,sizeof digest);
}

static bool bu_phy_tx_inhibited(struct xhci_device *d) {
    uint32_t cr,pause;
    return bu_read(d,BU_CR,1,&cr) && !cr && bu_read(d,BU_TXPAUSE,1,&pause) && pause==0xff;
}

static bool bu_phy_reassert_tx_pause(struct xhci_device *d,const char *phase) {
    uint32_t cr=UINT32_MAX,before=UINT32_MAX,after=UINT32_MAX;
    bool ctrl=bu_read(d,BU_CR,1,&cr);
    bool read=bu_read(d,BU_TXPAUSE,1,&before);
    bool ok=ctrl && !cr && read && (before==0xff || bu_write(d,BU_TXPAUSE,1,0xff)) &&
        bu_read(d,BU_CR,1,&cr) && !cr && bu_read(d,BU_TXPAUSE,1,&after) && after==0xff;
    console_printf("rtl8822bu-tx-pause: phase=%s cr=%x before=%x after=%x verified=%u\n",
        phase,(uint64_t)cr,(uint64_t)before,(uint64_t)after,(uint64_t)ok);
    return ok;
}

static bool bu_phy_powered(struct xhci_device *d) {
    uint32_t bb,rf,wlrf;
    return bu_read(d,2,1,&bb) && (bb&3)==3 && bu_read(d,0x1f,1,&rf) && (rf&7)==7 &&
           bu_read(d,BU_WLRF1,4,&wlrf) && (wlrf&BU_WLRF1_BBRF)==BU_WLRF1_BBRF;
}

static bool bu_phy_park(struct bu_upload *up) {
    struct xhci_device *d=up->device;
    if(!wifi_hub_check_port(d,up->hub))return false;
    /* Attempt each gate even if a preceding write was refused. */
    bool wlrf=bu_update(d,BU_WLRF1,4,BU_WLRF1_BBRF,0);
    bool rf=bu_update(d,0x1f,1,7,0);
    bool bb=bu_update(d,2,1,3,0);
    bool off=bu_radio_off(d);
    return wlrf && rf && bb && off && bu_phy_tx_inhibited(d);
}

static const char *bu_phy_run(struct bu_upload *up,const uint8_t *profile,unsigned *written,bool *touched) {
    struct xhci_device *d=up->device;
    if(!bu_phy_tx_inhibited(d) || !bu_radio_off(d))return "entry-gate";
    *touched=true;
    if(!bu_update(d,2,1,3,3) || !bu_update(d,0x1f,1,7,7) ||
       !bu_update(d,BU_WLRF1,4,BU_WLRF1_BBRF,BU_WLRF1_BBRF) || !bu_phy_powered(d))return "power";
    if(!bu_update(d,BU_RXPSEL,4,BU_RXPSEL_RESET,0))return "reset";
    uint64_t start=timer_monotonic_us();
    for(unsigned i=0;i<BU_PHY_ROWS;i++,profile+=BU_PHY_ROW_BYTES) {
        if(timer_monotonic_us()-start>=3000000u)return "deadline";
        if(i%64==0 && (!wifi_hub_check_port(d,up->hub) || !bu_phy_tx_inhibited(d)))return "table-gate";
        /* The USB trace shows about 204 ms at each RF standby transition. */
        if((i==2333 || i==2696) && !timer_delay_us(200000))return "rf-delay";
        unsigned reg=profile[0]|(unsigned)profile[1]<<8;
        uint32_t value=0;
        for(unsigned j=0;j<4;j++)value|=(uint32_t)profile[3+j]<<(8*j);
        if(!bu_write(d,reg,profile[2],value)) {
            console_printf("rtl8822bu-phy-write: row=%u reg=%x failed=1\n",(uint64_t)i,(uint64_t)reg);
            return "table-write";
        }
        (*written)++;
    }
    if(!bu_update(d,0x24,4,0x7e000000,0x28u<<25) || !bu_update(d,0x28,4,0x7e,0x28u<<1) ||
       !bu_update(d,BU_RXPSEL,4,BU_RXPSEL_RESET,BU_RXPSEL_RESET))return "crystal-setup";
    uint32_t xtal_a,xtal_b,rxpsel;
    if(!bu_read(d,0x24,4,&xtal_a) || !bu_read(d,0x28,4,&xtal_b) ||
       !bu_read(d,BU_RXPSEL,4,&rxpsel))return "readback";
    xtal_a=(xtal_a>>25)&0x3f;
    xtal_b=(xtal_b>>1)&0x3f;
    console_printf("rtl8822bu-phy-readback: xtal_a=%x xtal_b=%x rxpsel=%x\n",
        (uint64_t)xtal_a,(uint64_t)xtal_b,(uint64_t)rxpsel);
    if(xtal_a!=0x28 || xtal_b!=0x28 || (rxpsel&BU_RXPSEL_RESET)!=BU_RXPSEL_RESET)return "readback";
    return bu_phy_powered(d) && bu_phy_tx_inhibited(d) && wifi_hub_check_port(d,up->hub)?NULL:"exit-gate";
}
