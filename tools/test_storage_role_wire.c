/* Exercise actual row emitters against their complete decimal byte budget. */
#define SRVROS_WORKSPACE_HOST_TEST
#define SRVROS_STORAGE_COMPARE_HOST_TEST
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "../kernel/src/workspace.c"
static uint64_t emitted;
static unsigned line_bytes,max_line;
void console_printf(const char *fmt,...) {
    va_list args;va_start(args,fmt);
    for(const char *p=fmt;*p;p++) {
        unsigned n=1;
        if(*p=='%') {
            p++;assert(*p=='u');uint64_t v=va_arg(args,uint64_t);char out[32];
            int got=snprintf(out,sizeof(out),"%"PRIu64,v);assert(got>0);n=(unsigned)got;
        }
        emitted+=n;line_bytes+=n;
        if(*p=='\n'){if(line_bytes>max_line)max_line=line_bytes;line_bytes=0;}
    }
    va_end(args);
}
int main(void) {
    struct storage_compare_sample r={.sequence=12,.kind=1,.start=UINT64_MAX,.end=UINT64_MAX,
        .write_us=UINT64_MAX,.flush_us=UINT64_MAX,.read_us=UINT64_MAX,.crc=UINT64_MAX,.offset=UINT64_MAX,.ok=1,.retired=1};
    r.sidecar_checks=r.sidecar_equal=r.sidecar_changed=r.sidecar_shape=r.sidecar_fields=r.sidecar_offset=UINT64_MAX;r.sidecar_valid=1;
    r.metadata=(struct exfat_metadata_audit){.valid=1,.old_bytes=UINT64_MAX,.new_bytes=UINT64_MAX,.old_rows=UINT64_MAX,.new_rows=UINT64_MAX,.matched=UINT64_MAX,.added=UINT64_MAX,.removed=UINT64_MAX,.changed=UINT64_MAX,.subject_fields=UINT64_MAX,.marker_fields=UINT64_MAX,.representation=UINT64_MAX};
    for(unsigned i=0;i<8;i++)r.metadata.fields[i]=UINT64_MAX;
    r.usb.valid=true;
    for(unsigned i=0;i<3;i++)r.usb.calls[i]=r.usb.us[i]=UINT64_MAX;
    r.content.valid=1;r.content.command=(struct usb_storage_command_timing){.valid=1,.complete=1,.ok=1,.role=6,.start_us=UINT64_MAX,.end_us=UINT64_MAX,.csw_start_us=UINT64_MAX,.csw_end_us=UINT64_MAX,.checkpoint_calls=UINT64_MAX,.checkpoint_us=UINT64_MAX};
    for(unsigned i=0;i<3;i++)r.content.command.phase[i]=(struct usb_storage_command_phase){.calls=UINT64_MAX,.requested=UINT64_MAX,.actual=UINT64_MAX,.us=UINT64_MAX};
    r.usb.failures=UINT64_MAX;
    r.flush_io.valid=r.read_io.valid=true;r.flush_io.calls[2]=r.read_io.calls[2]=UINT64_MAX;
    r.usb.csw=(struct usb_storage_csw_timing){.start_us=UINT64_MAX,.end_us=UINT64_MAX,.opcode=UINT64_MAX,.command_bytes=UINT64_MAX,.request_bytes=UINT64_MAX,.actual=UINT64_MAX,.ok=UINT64_MAX,
        .sample={.submitted=UINT64_MAX,.observed=UINT64_MAX,.resumed=UINT64_MAX,.service_checks=UINT64_MAX,.service_samples=UINT64_MAX,.service_gap_max_us=UINT64_MAX,.completion_gap_us=UINT64_MAX,.empty_checks=UINT64_MAX,.event_checks=UINT64_MAX,.last_empty_sample_us=UINT64_MAX,.seen=true,.service_valid=true}};
    for(unsigned i=0;i<7;i++)r.content.roles[i]=(struct usb_storage_content_role){.read_commands=UINT64_MAX,.read_bytes=UINT64_MAX,.write_commands=UINT64_MAX,.write_bytes=UINT64_MAX,.flush_commands=UINT64_MAX,.other_commands=UINT64_MAX,.failed_commands=UINT64_MAX,.csw_calls=UINT64_MAX,.csw_us=UINT64_MAX,.write512_commands=UINT64_MAX,.write_csw_us=UINT64_MAX,.write512_csw_us=UINT64_MAX,.over100ms=UINT64_MAX,.write_over100ms=UINT64_MAX,.write512_over100ms=UINT64_MAX,.worst={.end_us=UINT64_MAX}};
    uint64_t wire=0;assert(compare_emit(&r,&wire)&&compare_emit_roles(&r,&wire)&&compare_emit_metadata(&r,&wire));
    assert(compare_emit(&r,NULL)&&compare_emit_roles(&r,NULL)&&compare_emit_metadata(&r,NULL));assert(wire==emitted&&max_line<1024&&!line_bytes);
    uint64_t one=wire;unsigned accepted=1;
    while(compare_emit(&r,&wire)&&compare_emit_roles(&r,&wire)&&compare_emit_metadata(&r,&wire))accepted++;
    assert(accepted<12&&wire<=30720); /* Overflow refuses; no output was attempted. */
    assert(emitted==one);wire=30720;assert(!compare_emit(&r,&wire)&&wire==30720);
    printf("storage-role-wire: PASS actual maximum-width rows, exact byte count, line limit and full-receipt overflow refusal (%u complete samples)\n",accepted);
    return 0;
}
