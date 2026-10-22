/* Explicit firmware diagnostic only. No network provider or RF setup. */
static bool wifi_firmware_owned(struct xhci_device *d) {
    return d->firmware.active && d->firmware.configured && !d->firmware.failed &&
        !d->ep0_quarantined && d->wifi_probe_ok &&
        d->wifi_identity.asic==0x76100002 && d->wifi_identity.mac==0x76502000;
}
static bool wifi_firmware_fail(struct xhci_device *d) {
    d->firmware.failed=true;d->ep0_quarantined=true;return false;
}
static bool wifi_firmware_event(struct xhci_device *d,const struct xhci_trb *event) {
    struct xhci_wifi_firmware *f=&d->firmware;
    uint64_t pointer=event->parameter_low|((uint64_t)event->parameter_high<<32);
    if(f->active && f->stop_attempted && ((event->control>>16)&31)==1) {
        /* Preserve the first late/stopped event, even if its pointer is outside
         * our submitted TD. This is evidence only, never successful retirement. */
        if(!f->stop_event_cc) {
            f->stop_event_pointer=pointer;f->stop_event_cc=event->status>>24;
            f->stop_event_residue=event->status&0xffffff;
        }
        return true;
    }
    if(!f->pending || ((event->control>>16)&31)!=16 || pointer!=f->trb)return false;
    f->completion=event->status>>24;f->remaining=event->status&0xffffff;
    f->done=true;f->pending=false;
    if((event->control&4) || f->completion!=1 || f->remaining)wifi_firmware_fail(d);
    return true;
}
static bool configure_wifi_firmware(struct xhci_device *d) {
    struct xhci_wifi_firmware *f=&d->firmware;
    if(d->wifi_identity.asic!=0x76100002 || d->wifi_identity.mac!=0x76502000 ||
        d->network || d->configured || d->ep0_quarantined || f->allocation)return false;
    void *virt;
    if(!alloc_frame_zero(&f->ep.ring_phys,&virt))return false;
    f->ep.ring=virt;f->ep.cycle=true;f->ep.dci=16;f->ep.packet=512;
    write_link_trb(f->ep.ring,f->ep.ring_phys,true);
    /* A 16K-aligned 16K window inside eight frames never crosses a 64K TRB
     * boundary. Retain all allocations even after uncertain configure errors. */
    f->allocation=pmm_alloc_frames_tagged(8,PMM_FRAME_DMA);
    if(!f->allocation)return false;
    f->buffer_phys=(f->allocation+16383)&~16383ull;
    f->buffer=pmm_phys_to_virt(f->buffer_phys);
    f->scratch_phys=pmm_alloc_frames_tagged(4,PMM_FRAME_DMA);
    if(!f->scratch_phys)return false;
    f->scratch=pmm_phys_to_virt(f->scratch_phys);
    struct usb_setup_packet setup={.request_type=0,.request=9,.value=1};int actual;
    if(!control_transfer_internal(d,&setup,0,0,false,&actual))return false;
    zero_bytes(d->input_context,PMM_FRAME_SIZE);
    context_write32(d->input_context,0,1,1u|(1u<<16));
    uint32_t slot=context_read32(d->output_context,0,0);
    context_write32(d->input_context,1,0,(slot&~(31u<<27))|(16u<<27));
    for(unsigned i=1;i<3;i++)context_write32(d->input_context,1,i,context_read32(d->output_context,0,i));
    context_write32(d->input_context,17,1,(3u<<1)|(2u<<3)|(512u<<16));
    context_write32(d->input_context,17,2,(uint32_t)(f->ep.ring_phys|1));
    context_write32(d->input_context,17,3,(uint32_t)(f->ep.ring_phys>>32));
    context_write32(d->input_context,17,4,MT76_FW_PACKET_MAX);
    struct xhci_trb cmd={.parameter_low=(uint32_t)d->input_context_phys,.parameter_high=(uint32_t)(d->input_context_phys>>32),
        .control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)d->slot_id<<24)};
    uint64_t flags=event_irq_save(),returned_slot=0;
    firmware_command_exact=true;
    bool ok=submit_command_trb(&cmd,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,&returned_slot);
    firmware_command_exact=false;
    if(!ok || returned_slot!=d->slot_id)firmware_command_failed=true;
    event_irq_restore(flags);
    if(firmware_command_failed)return false;
    f->configured=true;d->stage="wifi-firmware-endpoint";return true;
}
/* Stop a timed-out owned EP0 exactly once so the output context becomes a
 * stopped-endpoint snapshot. No reset, Set Dequeue, restart, retry or DMA free.
 * A failed command quarantines further controller commands as configuration does. */
static void wifi_fw_stop_timeout(struct xhci_device *d) {
    struct xhci_wifi_firmware *f=&d->firmware;
    if(!f->active || !f->configured || !f->control_timeout || !d->ep0_quarantined ||
        f->stop_attempted || !xhci.command_ring || firmware_command_failed ||
        strict_control.device || !d->output_context) return;
    uint64_t flags=event_irq_save(),slot=0;
    f->stop_attempted=true;
    struct xhci_trb cmd={.control=(15u<<10)|(1u<<16)|((uint32_t)d->slot_id<<24)};
    firmware_command_exact=true;
    f->stop_ok=submit_command_trb(&cmd,15,&slot) && slot==d->slot_id;
    firmware_command_exact=false;
    if(!f->stop_ok)firmware_command_failed=true;
    if(f->stop_ok) {
        f->stop_state=context_read32(d->output_context,1,0)&7;
        f->stop_dequeue=context_read32(d->output_context,1,2)|
            ((uint64_t)context_read32(d->output_context,1,3)<<32);
    }
    event_irq_restore(flags);
}
/* Cache diagnostic metadata only; no extra transfer, retry or poll logging. */
static bool wifi_fw_control(struct xhci_device *d,const struct usb_setup_packet *setup,
    uint64_t buffer,uint32_t length,bool in,int *actual) {
    struct xhci_wifi_firmware *f=&d->firmware;
    f->control_count++;f->control_type=setup->request_type;f->control_request=setup->request;
    f->control_value=setup->value;f->control_index=setup->index;f->control_length=setup->length;
    f->control_cc=f->control_residue=f->control_stage=0;f->control_timeout=false;
    f->control_elapsed_us=0;f->control_ep_state=0;f->control_dequeue=0;
    f->control_ok=control_transfer_internal(d,setup,buffer,length,in,actual);
    f->control_actual=*actual;
    if(d->output_context) {
        f->control_ep_state=context_read32(d->output_context,1,0)&7;
        f->control_dequeue=context_read32(d->output_context,1,2)|
            ((uint64_t)context_read32(d->output_context,1,3)<<32);
    }
    if(f->control_timeout)wifi_fw_stop_timeout(d);
    return f->control_ok;
}
static bool wifi_fw_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;*value=0;
    if(!wifi_firmware_owned(d) || (address!=0x80 && address!=0x20 && address!=0x730 && address!=0x238 && address!=0x1004 && address!=0x9a8))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_firmware_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    d->firmware.last_read_address=address;d->firmware.last_read_value=*value;d->firmware.read_count++;
    if(address==0x80){
        if(!d->firmware.wlan_written){d->firmware.wlan_before=*value;d->firmware.wlan_valid=true;}
        else d->firmware.wlan_after=*value;
    }
    if(address==0x20)d->firmware.cmb=*value;
    if(address==0x238){d->firmware.cfg=*value;d->firmware.cfg_valid=true;}
    if(address==0x9a8){d->firmware.descriptor=*value;d->firmware.descriptor_valid=true;}
    return true;
}
static bool wifi_fw_write(void *context,uint32_t address,uint32_t value) {
    struct xhci_device *d=context;struct xhci_wifi_firmware *f=&d->firmware;
    if(!wifi_firmware_owned(d))return false;
    bool allowed=(address==0x1004 && value==0x20) || (address==0x800 && value==1) ||
        (address==0x9a0 && value==0x400230) || (address==0x9a4 && value==1) ||
        (address==0x9c4 && value==0x44) || (address==0xa6c && value==3);
    if(address==0x1004 && value==0x2c)allowed=wifi_mac_startup && f->preparation.mac_startup &&
        f->preparation.attempted && !f->preparation.prepared && !f->reset_seen &&
        !f->upload.attempted && !f->mac_startup_written;
    if(address==0x80)allowed=f->wlan_valid && !f->wlan_written &&
        f->wlan_before!=0xffffffffu && !(f->wlan_before&0xcu) && value==(f->wlan_before|3u);
    if(address==0x238)allowed=value==0xc00000 || (f->cfg_valid &&
        (value==(f->cfg|0xc00020) || value==(f->cfg|0x10000) || value==(f->cfg&~0x10000u)) && !(value&0xc0000000u));
    if(address==0x9a8)allowed=f->phase==3 && f->descriptor_valid && value==f->descriptor+1;
    if(!allowed)return false;
    for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=(uint8_t)(value>>(8*i));
    struct usb_setup_packet setup={.request_type=0x40,.request=6,.index=(uint16_t)address,.length=4};int actual;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,false,&actual) || actual!=4)return wifi_firmware_fail(d);
    if(address==0x80)f->wlan_written=true;
    if(address==0x1004 && value==0x2c)f->mac_startup_written=true;
    if(address==0x9a8){f->phase=0;f->chunk++;f->descriptor_valid=false;}
    return true;
}
static bool wifi_fw_reset(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_firmware *f=&d->firmware;
    if(!wifi_firmware_owned(d) || f->reset_seen || f->preparation.prepared || f->upload.attempted)return false;
    f->reset_seen=true;
    struct usb_setup_packet setup={.request_type=0x40,.request=1,.value=1};int actual;
    if(!wifi_fw_control(d,&setup,0,0,false,&actual))return wifi_firmware_fail(d);
    return true;
}
static bool wifi_fw_delay(void *context,uint32_t us) {
    return wifi_firmware_owned(context) && us<=5000 && timer_delay_us(us);
}
static bool wifi_fw_authorize(void *context,const uint8_t *image,size_t size,const struct mt76_identity *identity) {
    struct xhci_device *d=context;struct xhci_wifi_firmware *f=&d->firmware;
    return wifi_firmware_owned(d) && f->preparation.prepared && identity==&d->wifi_identity &&
        image==f->image && size==f->image_size && mt76_firmware_verified(image,size);
}
static bool wifi_fw_fce(void *context,uint32_t address,uint32_t value) {
    struct xhci_device *d=context;struct xhci_wifi_firmware *f=&d->firmware;struct mt76_fw_chunk chunk;
    if(!wifi_firmware_owned(d) || !f->preparation.prepared || !f->upload.attempted ||
        !mt76_firmware_chunk(f->image,f->image_size,f->chunk,&chunk) || chunk.segment==MT76_FW_IVB ||
        !((f->phase==0 && address==0x230 && value==chunk.destination) ||
          (f->phase==1 && address==0x234 && value==chunk.padded_bytes<<16)))return false;
    for(unsigned half=0;half<2;half++) {
        struct usb_setup_packet setup={.request_type=0x40,.request=0x42,.value=(uint16_t)(value>>(16*half)),.index=(uint16_t)(address+2*half)};int actual;
        if(!wifi_fw_control(d,&setup,0,0,false,&actual))return wifi_firmware_fail(d);
    }
    f->phase++;return true;
}
static int wifi_fw_bulk(void *context,const uint8_t *data,size_t size) {
    struct xhci_device *d=context;struct xhci_wifi_firmware *f=&d->firmware;struct mt76_fw_chunk chunk;
    if(!wifi_firmware_owned(d) || f->phase!=2 || f->pending || data!=f->scratch ||
        !mt76_firmware_chunk(f->image,f->image_size,f->chunk,&chunk) || chunk.segment==MT76_FW_IVB || size!=chunk.wire_bytes || size>MT76_FW_PACKET_MAX)return -1;
    copy_bytes(f->buffer,data,size);
    struct xhci_trb trb={.parameter_low=(uint32_t)f->buffer_phys,.parameter_high=(uint32_t)(f->buffer_phys>>32),
        .status=(uint32_t)size,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    uint64_t flags=event_irq_save();
    f->trb=ring_push(f->ep.ring,f->ep.ring_phys,&f->ep.enqueue,&f->ep.cycle,&trb);
    f->pending=true;f->done=false;f->completion=0;f->remaining=0;db_write32(d->slot_id,16);
    event_irq_restore(flags);
    uint64_t start=timer_monotonic_us(),clock=start;unsigned stalled=0;
    for(;;) {
        flags=event_irq_save();struct xhci_trb event;
        if(!f->done && poll_event(&event))dispatch_transfer_event(&event);
        bool done=f->done;event_irq_restore(flags);
        if(done){if(f->failed)return -1;f->phase=3;return (int)size;}
        uint64_t now=timer_monotonic_us();
        if(now-start>=2000000 || (now==clock && ++stalled>=XHCI_TRANSFER_TIMEOUT))break;
        if(now!=clock){clock=now;stalled=0;}
        __asm__ volatile("pause");
    }
    wifi_firmware_fail(d);return -1;
}
static int wifi_fw_activate(void *context,const uint8_t *data,size_t size) {
    struct xhci_device *d=context;struct xhci_wifi_firmware *f=&d->firmware;struct mt76_fw_chunk chunk;
    if(!wifi_firmware_owned(d) || f->phase || !mt76_firmware_chunk(f->image,f->image_size,f->chunk,&chunk) ||
        chunk.segment!=MT76_FW_IVB || data!=f->image+32 || size!=64)return -1;
    copy_bytes(d->descriptor_buffer,data,64);
    struct usb_setup_packet setup={.request_type=0x40,.request=1,.value=0x12,.length=64};int actual;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,64,false,&actual) || actual!=64){wifi_firmware_fail(d);return -1;}
    f->phase=4;return 64;
}
bool xhci_wifi_capture_firmware(const uint8_t *image,uint64_t size) {
    if(!wifi_firmware_enabled || !mt76_firmware_verified(image,size)) {
        console_write("wifi-firmware: refused missing/unpinned image\n");return false;
    }
    unsigned found=0;bool success=true;
    for(unsigned i=0;i<XHCI_MAX_DEVICES;i++) {
        struct xhci_device *d=&xhci.devices[i];struct xhci_wifi_firmware *f=&d->firmware;
        if(!d->present || !d->wifi_probe_attempted)continue;
        found++;
        if(f->attempted || !f->configured || f->failed || d->ep0_quarantined){success=false;continue;}
        f->attempted=true;f->active=true;f->image=image;f->image_size=size;
        f->preparation.mac_startup=wifi_mac_startup;
        struct mt76_prepare_io prep={{d,wifi_fw_read,wifi_fw_write,wifi_fw_delay},wifi_fw_reset};
        enum mt76_upload_status status=mt76_prepare_firmware(&f->preparation,&prep,&d->wifi_identity);
        if(status==MT76_UPLOAD_OK) {
            struct mt76_firmware_io io={d,wifi_fw_authorize,wifi_fw_fce,wifi_fw_read,wifi_fw_write,wifi_fw_bulk,wifi_fw_activate,wifi_fw_delay};
            status=mt76_upload_firmware(&f->upload,&io,&d->wifi_identity,image,size,f->scratch,MT76_FW_PACKET_MAX);
        }
        uint32_t mac=0;
        bool disabled=false;
        if(status==MT76_UPLOAD_OK) {
            disabled=wifi_firmware_owned(d) && wifi_fw_read(d,0x1004,&mac) && mac==0x20;
            /* A successful activation which changed MAC state is a failed
             * diagnostic; make one bounded attempt to disable MAC again. */
            if(!disabled && wifi_firmware_owned(d))wifi_fw_write(d,0x1004,0x20);
        }
        /* Report cached state after the attempt, without adding hardware reads or
         * logging inside readiness polling. This distinguishes refusal gates. */
        console_printf("wifi-firmware-control: count=%u type=%x request=%x value=%x index=%x length=%u\nwifi-firmware-control-result: actual=%d ok=%u cc=%u residue=%u stage=%u timeout=%u\n",
            (uint64_t)f->control_count,(uint64_t)f->control_type,(uint64_t)f->control_request,
            (uint64_t)f->control_value,(uint64_t)f->control_index,(uint64_t)f->control_length,
            (int64_t)f->control_actual,(uint64_t)f->control_ok,(uint64_t)f->control_cc,
            (uint64_t)f->control_residue,(uint64_t)f->control_stage,(uint64_t)f->control_timeout);
        console_printf("wifi-firmware-endpoint: elapsed_us=%u state=%u dequeue=%x\n",
            f->control_elapsed_us,(uint64_t)f->control_ep_state,f->control_dequeue);
        console_printf("wifi-firmware-td: setup=%x data=%x status=%x\n",
            f->control_setup,f->control_data,f->control_status);
        console_printf("wifi-firmware-stop: attempted=%u ok=%u state=%u dequeue=%x event=%x cc=%u residue=%u\n",
            (uint64_t)f->stop_attempted,(uint64_t)f->stop_ok,(uint64_t)f->stop_state,
            f->stop_dequeue,f->stop_event_pointer,(uint64_t)f->stop_event_cc,(uint64_t)f->stop_event_residue);
        console_printf("wifi-firmware-mac-startup: requested=%u written=%u\n",
            (uint64_t)f->preparation.mac_startup,(uint64_t)f->mac_startup_written);
        console_printf("wifi-firmware-clock: before=%x after=%x cmb=%x written=%u\n",
            (uint64_t)f->wlan_before,(uint64_t)f->wlan_after,(uint64_t)f->cmb,(uint64_t)f->wlan_written);
        console_printf("wifi-firmware-progress: reads=%u last_register=%x last_value=%x prepared=%u reset=%u upload=%u\n",
            (uint64_t)f->read_count,(uint64_t)f->last_read_address,(uint64_t)f->last_read_value,
            (uint64_t)f->preparation.prepared,(uint64_t)f->reset_seen,(uint64_t)f->upload.attempted);
        if(status!=MT76_UPLOAD_OK || !disabled){wifi_firmware_fail(d);success=false;}
        console_printf("wifi-firmware: slot=%u result=%u chunks=%u mac_disabled=%u quarantined=%u\n",
            (uint64_t)d->slot_id,(uint64_t)status,(uint64_t)f->chunk,(uint64_t)disabled,(uint64_t)f->failed);
        f->active=false;
    }
    return found && success;
}
