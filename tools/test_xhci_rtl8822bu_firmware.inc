enum bu_firmware_fault {
    BU_FW_OK,BU_FW_IMAGE,BU_FW_WARM,BU_FW_SHORT,BU_FW_MISSING,BU_FW_CHECKSUM,
    BU_FW_NOT_READY,BU_FW_BAD_STOP,BU_FW_BUSY,
};

static struct {
    const uint8_t *image;
    uint8_t reg[65536];
    uint64_t ring,cursor,packet;
    uint32_t written;
    unsigned posted,ddma,configure,stop,echoes;
    uint8_t echo;
    bool echo_expected;
    enum bu_firmware_fault fault;
} bu_model;

static uint32_t bu_model_read(unsigned reg,unsigned width) {
    uint32_t value=0;
    for(unsigned i=0;i<width;i++)value|=(uint32_t)bu_model.reg[reg+i]<<(8*i);
    return value;
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
        assert(!bu_model.configure++ && context_read32(d->input_context,0,1)==(1u|(1u<<10)));
        assert(context_read32(d->input_context,1,0)>>27==10);
        assert(context_read32(d->input_context,11,1)==(6u|(2u<<3)|(512u<<16)));
        bu_model.ring=context_read32(d->input_context,11,2) |
                     (uint64_t)context_read32(d->input_context,11,3)<<32;
        assert((bu_model.ring&4095)==1);
        bu_model.cursor=bu_model.ring;
    } else {
        assert(type==XHCI_TRB_TYPE_STOP_ENDPOINT && ((command->control>>16)&31)==10 && !bu_model.stop++);
        uint64_t cursor=bu_model.cursor;
        if(bu_model.fault==BU_FW_BAD_STOP)cursor+=32;
        context_write32(d->output_context,10,0,3);
        context_write32(d->output_context,10,2,(uint32_t)cursor);
        context_write32(d->output_context,10,3,cursor>>32);
    }
    uint64_t pointer=xhci.command_ring_phys+idx*16;
    xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)pointer,
        .parameter_high=pointer>>32,.status=1u<<24,.control=(33u<<10)|(4u<<24)|xhci.event_cycle};
}

static unsigned bu_model_block_size(unsigned index) {
    unsigned end=index<3?11216:161176;
    return end-bu_model.written>4096?4096:end-bu_model.written;
}

static void bu_model_bulk(struct xhci_device *d) {
    assert(bu_model.configure==1 && !bu_model.stop && bu_model.posted==bu_model.ddma);
    assert(bu_model_read(0x100,2)==0x105 && !(bu_model_read(0x550,1)&8) &&
           (bu_model_read(0x80,1)&1) && !(bu_model_read(3,1)&4) && !(bu_model_read(0x1d,1)&1));
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
    unsigned cc=bu_model.fault==BU_FW_SHORT?13:1;
    xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){
        .parameter_low=(uint32_t)d->bulk_trb,.parameter_high=d->bulk_trb>>32,
        .status=(cc<<24)|(cc==13?1:0),.control=(32u<<10)|(4u<<24)|(10u<<16)|xhci.event_cycle};
    bu_model_store(0x204,2,0x8000);
}

static void bu_model_control(void) {
    struct xhci_device *d=&xhci.devices[3];
    assert(strict_control.device==d);
    const struct xhci_trb *setup=(const void *)(uintptr_t)strict_control.setup;
    unsigned reg=setup->parameter_low>>16,width=setup->parameter_high>>16;
    bool in=strict_control.data_in;
    assert((setup->parameter_low&0xffff)==(in?0x05c0:0x0540) && !(setup->parameter_high&0xffff));
    assert((width==1 || width==2 || width==4) && reg+width<=sizeof bu_model.reg);
    if(bu_model.echo_expected) {
        assert(!in && reg==0x4e0 && width==1 && d->descriptor_buffer[0]==bu_model.echo);
        bu_model.echo_expected=false;bu_model.echoes++;
    } else if(in) {
        memcpy(d->descriptor_buffer,bu_model.reg+reg,width);
    } else {
        uint32_t value=0;
        for(unsigned i=0;i<width;i++)value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
        if(reg==2)assert(!(value&3));
        if(reg==0x1f)assert(!(value&7));
        if(reg==0xec)assert(!(value&0x1c000007u));
        if(reg==0x100)assert(value==0 || value==5);
        bu_model_store(reg,width,value);
        if(reg==5 && (value&1)) {
            bu_model_store(reg,1,value&~1u);
            bu_model_store(0x100,1,0);
        }
        if(reg==0x204)bu_model_store(reg,width,0);
        if(reg==0x1700) {
            if(value==0xc00f0038 && bu_model.fault!=BU_FW_NOT_READY) {
                assert(bu_model.ddma==40 && bu_model.written==161176 && (bu_model_read(3,1)&4));
                bu_model_store(0x80,2,0xc078);
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
}

static void bu_model_mmio(uint64_t offset,uint32_t value) {
    struct xhci_device *d=&xhci.devices[3];
    if(offset==xhci.db_base && !value)bu_model_command(d);
    else if(offset==xhci.db_base+16 && value==10)bu_model_bulk(d);
    else if(offset==xhci.db_base+16 && value==1)bu_model_control();
    else rtl8822bu_probe_model(offset,value);
}

static bool bu_model_delay(uint64_t us) {
    assert(us==1000 || us==50);
    test_time+=us;
    return true;
}

static void test_rtl8822bu_firmware(const char *path) {
    static uint8_t image[BU_FW_BYTES];
    FILE *file=fopen(path,"rb");
    assert(file && fread(image,1,sizeof image,file)==sizeof image && fgetc(file)==EOF && !fclose(file));
    assert(bu_image_valid(image,sizeof image));
    for(enum bu_firmware_fault fault=BU_FW_OK;fault<=BU_FW_BUSY;fault++) {
        struct xhci_device *rtl=rtl_hub_fixture(),*d=&xhci.devices[3];
        d->device_bcd=0x0210;d->configuration_count=1;d->ep0_max_packet=64;
        uint8_t *command_memory;
        assert(allocate_device_runtime(d) && alloc_frame_zero(&xhci.command_ring_phys,(void **)&command_memory));
        xhci.command_ring=(void *)command_memory;xhci.command_cycle=true;
        test_time_step=1;bu_probe_case=BU_OK;bu_leaf_requests=bu_hub_reads=0;
        native_boundary_mmio_hook=rtl8822bu_probe_model;
        assert(xhci_rtl8822bu_capture_identity());
        memset(&bu_model,0,sizeof bu_model);bu_model.image=image;bu_model.fault=fault;
        bu_model_store(0x100,1,fault==BU_FW_WARM?0:0xea);
        bu_model_store(0x6,1,3);bu_model_store(0x80,2,0x78);
        bu_model_store(0x3,1,0xd8);bu_model_store(0x550,1,0x14);
        bu_model_store(0x1700,4,0x20000000);
        bu_model_store(0x1708,4,0xdd03);
        rtl_test_clock=true;rtl_test_delay=bu_model_delay;
        if(fault==BU_FW_MISSING)test_time_step=10000;
        native_boundary_mmio_hook=bu_model_mmio;
        if(fault==BU_FW_IMAGE)image[0]^=1;
        bool safe=false;
        assert(xhci_rtl8822bu_capture_firmware(image,sizeof image,&safe)==(fault==BU_FW_OK));
        if(fault==BU_FW_IMAGE)image[0]^=1;
        assert(safe==(fault!=BU_FW_BAD_STOP && fault!=BU_FW_BUSY));
        assert(!bu_model.echo_expected && !rtl->control_success && !xhci.devices[2].control_success && !network_count);
        if(fault==BU_FW_OK)assert(bu_model.posted==40 && bu_model.ddma==40 && !d->bulk_pending && bu_model.stop);
        if(fault==BU_FW_IMAGE)assert(!bu_model.posted && !bu_model.configure && !bu_model.echoes);
        if(fault==BU_FW_SHORT || fault==BU_FW_MISSING)assert(bu_model.posted==1 && !bu_model.ddma && !d->bulk_pending);
    }
    native_boundary_mmio_hook=NULL;rtl_test_delay=NULL;rtl_test_clock=false;
    puts("rtl8822bu: PASS native firmware upload, forty USB packets, both DDMA sections, MCU ready and endpoint retirement");
    puts("rtl8822bu: PASS image and warm refusal, short/missing OUT, checksum, missing ready, wrong stop and busy DDMA");
}
