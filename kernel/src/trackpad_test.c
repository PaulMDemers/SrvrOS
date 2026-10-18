/* Opt-in physical capture. Completion requires human gestures, never simulation. */
#include <srvros/trackpad_test.h>
#include <srvros/xhci.h>
#include <srvros/console.h>
#include <srvros/keyboard.h>
#include <srvros/timer.h>
#include <srvros/workspace.h>
#include <srvros/exfat.h>
#include <srvros/vfs.h>
#include <srvros/halt.h>
#include <srvros/process.h>
#include <srvros/mouse.h>
#include <stddef.h>

static char report[8192];
static unsigned used;
static bool overflow;
static bool equal(const char *a,const char *b) {
    while (*a && *a==*b) { a++; b++; }
    return *a==*b;
}
static unsigned length(const char *s) { unsigned n=0; while (s[n]) n++; return n; }
static void append(char *s,unsigned *n,const char *value) {
    while (*value) s[(*n)++]=*value++;
    s[*n]=0;
}
/* Return 0 absent, 1 valid, -1 malformed/duplicate. */
static int option(const char *cmdline,const char *key,char out[17]) {
    unsigned matches=0,k=length(key);
    for (const char *p=cmdline; p && *p;) {
        while (*p==' ' || *p=='\t' || *p=='\n') p++;
        const char *end=p; while (*end && *end!=' ' && *end!='\t' && *end!='\n') end++;
        unsigned i=0; while (i<k && p+i<end && p[i]==key[i]) i++;
        if (i==k) {
            if (++matches>1 || end-p!=k+16) return -1;
            for (i=0;i<16;i++) {
                char c=p[k+i];
                if (!((c>='0' && c<='9') || (c>='a' && c<='f'))) return -1;
                out[i]=c;
            }
            out[16]=0;
        }
        p=end;
    }
    return matches?1:0;
}
static void field(const char *key,const char *value) {
    if (used+length(key)+length(value)+2>=sizeof(report)) { overflow=true; return; }
    append(report,&used,key); append(report,&used,"="); append(report,&used,value); append(report,&used,"\n");
}
static void number(const char *key,uint64_t n) {
    char reverse[20],text[21]; unsigned count=0,w=0;
    do { reverse[count++]='0'+n%10; n/=10; } while (n);
    while (count) text[w++]=reverse[--count];
    text[w]=0; field(key,text);
}
static uint32_t crc32(const uint8_t *data,unsigned n) {
    uint32_t crc=0xffffffff;
    for (unsigned i=0;i<n;i++) {
        crc^=data[i];
        for (unsigned j=0;j<8;j++) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));
    }
    return ~crc;
}
static void path(char out[80],const char *run,const char *suffix) {
    unsigned n=0; append(out,&n,"/work/srvros-work/trackpad-"); append(out,&n,run); append(out,&n,suffix);
}
static bool file_equals(const char *name,const char *text,unsigned n) {
    const struct vfs_node *node=vfs_lookup(name); const uint8_t *data; uint64_t size;
    if (!node || node->type!=VFS_NODE_FILE || !vfs_read_all(node,&data,&size)) return false;
    bool ok=size==n;
    for (unsigned i=0;ok && i<n;i++) ok=data[i]==(uint8_t)text[i];
    vfs_release_data(node,data); return ok;
}
static bool coverage(const struct xhci_trackpad_capture *s,bool keyboard) {
    return s->found && s->ready && s->reports && s->contact_reports && s->max_contacts>=2 &&
        s->button_reports && s->lifts && !s->errors && !s->malformed && keyboard;
}
static void build_report(const char *run,const char *release,const struct xhci_trackpad_capture *s,
    const char *input,uint64_t keyboard_before,uint64_t dropped,uint64_t elapsed,int64_t desktop_exit) {
    used=0; overflow=false;
    bool keyboard=equal(input,"AaZz09") && s->keyboard_reports>keyboard_before && !dropped;
    field("format","srvros-trackpad-v1"); field("run",run); field("release",release);
    number("found",s->found); number("ready",s->ready); number("slot",s->slot);
    number("reports",s->reports); number("contact_reports",s->contact_reports);
    number("button_reports",s->button_reports); number("lifts",s->lifts);
    number("max_contacts",s->max_contacts); number("malformed",s->malformed); number("errors",s->errors);
    number("request_bytes",APPLE_TRACKPAD_REPORT_MAX); number("last_bytes",s->last_length);
    number("last_completion",s->completion); number("keyboard_reports_before",keyboard_before);
    number("keyboard_reports_after",s->keyboard_reports); number("keyboard_dropped",dropped);
    number("elapsed_us",elapsed); field("typed",input); field("keyboard",keyboard?"PASS":"INCOMPLETE");
    const char *names[]={"sample_first","sample_touch","sample_multi","sample_button","sample_malformed"};
    for (unsigned i=0;i<XHCI_TRACKPAD_SAMPLES;i++) {
        char hex[APPLE_TRACKPAD_REPORT_MAX*2+1]; unsigned n=s->samples[i].length;
        if (n>APPLE_TRACKPAD_REPORT_MAX) { overflow=true; return; }
        for (unsigned j=0;j<n;j++) {
            hex[2*j]="0123456789abcdef"[s->samples[i].data[j]>>4];
            hex[2*j+1]="0123456789abcdef"[s->samples[i].data[j]&15];
        }
        hex[2*n]=0; field(names[i],hex);
    }
    if (s->pointer_enabled) {
        number("pointer_queue_overflows",s->pointer_queue_overflows);
        number("pointer_motion",s->pointer_motion); number("pointer_buttons",s->pointer_buttons);
        number("pointer_cancels",s->pointer_cancels);
        field("desktop_exit",desktop_exit==0?"PASS":"FAIL");
        bool exercised=coverage(s,keyboard) && desktop_exit==0 && s->pointer_motion &&
            s->pointer_buttons>=2 && !s->pointer_cancels && !s->pointer_queue_overflows;
        field("pointer_trial",exercised?"EXERCISED":"INCOMPLETE");
        if (s->gestures_enabled) {
            number("gesture_scroll_events",s->scroll_events);
            number("gesture_typing_suppressed",s->typing_suppressed);
            number("gesture_palm_suppressed",s->palm_suppressed);
            number("gesture_max_touch_major",s->max_touch_major);
            number("gesture_max_touch_minor",s->max_touch_minor);
            field("gesture_trial",exercised && s->scroll_events?"EXERCISED":"INCOMPLETE");
        }
    }
    field("status",coverage(s,keyboard)?"CAPTURED":"INCOMPLETE");
    number("crc32",crc32((const uint8_t *)report,used));
    field("complete",run);
}
static void screen(const char *run,const struct xhci_trackpad_capture *s,const char *input) {
    console_clear();
    console_write("SrvrOS physical trackpad capture\n\n");
    if (s->gestures_enabled) {
        console_write("LIVE FILTER CHECKS - no timer; take your time\n");
        console_write("1. Hold A while brushing the pad lightly WITHOUT pressing it.\n");
        console_write("   TYPING should change to EXERCISED. Release A and lift fully.\n");
        console_write("2. Without typing, rest a palm edge on the pad, then lift fully.\n");
        console_write("   BROAD may change to EXERCISED; note if it never does.\n");
        console_write("   This provisional filter may miss palms; do not force a pass.\n");
        console_write("3. Swipe with one finger, then two; physically click several times.\n");
        console_write("4. Press Escape to CLEAR the typing below. Type AaZz09, then Enter.\n\n");
        console_printf("TYPING: %s (%u frames)   BROAD: %s (%u frames)\n",
            s->typing_suppressed?"EXERCISED":"NOT YET",s->typing_suppressed,
            s->palm_suppressed?"EXERCISED":"NOT YET",s->palm_suppressed);
        console_printf("Max touch major=%u minor=%u; broad threshold >600 (or >2 contacts)\n",
            (uint64_t)s->max_touch_major,(uint64_t)s->max_touch_minor);
        console_printf("Scroll events=%u motion=%u button edges=%u\n\n",
            s->scroll_events,s->pointer_motion,s->pointer_buttons);
        console_write("Counters cover desktop + this screen; EXERCISED does not mean good feel.\n");
    } else {
    console_write("1. Swipe with ONE finger, then lift it.\n");
    console_write("2. Swipe with TWO fingers, then lift them.\n");
    console_write("3. PRESS the trackpad to click several times (not just tap).\n");
    console_write("4. Type AaZz09 on the built-in keyboard, then press Enter.\n\n");
    }
    console_write(s->pointer_enabled?"Desktop trial ended; complete these checks to save.\n":
        "This captures reports only; the pointer will not move.\n");
    console_write("Enter saves the USB report and HALTS for a manual restart.\n\n");
    console_printf("Run: %s\nDevice found=%u ready=%u reports=%u\n",
        run,(uint64_t)s->found,(uint64_t)s->ready,s->reports);
    console_printf("Touch frames=%u max fingers=%u click frames=%u lifts=%u\n",
        s->contact_reports,(uint64_t)s->max_contacts,s->button_reports,s->lifts);
    console_printf("Malformed=%u transport errors=%u\nTyped: %s\n",s->malformed,s->errors,input);
    console_sync();
}
static void capture(const char *run,const char *release,bool desktop_io) {
    char request_path[80],result_path[80],done_path[80],request[64]; unsigned n=0;
    path(request_path,run,".req"); path(result_path,run,".txt"); path(done_path,run,".done");
    append(request,&n,"SRVROS-TRACKPAD-V1 "); append(request,&n,run); append(request,&n,"\n");
    if (!workspace_active() || vfs_lookup(result_path) || vfs_lookup(done_path) || !file_equals(request_path,request,n)) {
        console_write("trackpad-test: request absent/consumed, report exists, or workspace unavailable\n"); return;
    }
    n=0; append(request,&n,"STARTED "); append(request,&n,run); append(request,&n,"\n");
    if (!exfat_write_file(request_path,(const uint8_t *)request,n) || !workspace_sync() ||
        !file_equals(request_path,request,n)) {
        console_write("trackpad-test: request consumption/flush FAILED\n"); return;
    }
    struct xhci_trackpad_capture snapshot;
    xhci_trackpad_reset_capture(); xhci_trackpad_snapshot(&snapshot);
    uint64_t before=snapshot.keyboard_reports,start=timer_monotonic_us(),dropped=keyboard_dropped_count();
    char input[32]={0},c; unsigned count=0;
    while (keyboard_scan_char(&c)) {} /* Discard keys left over from boot. */
    int64_t desktop_exit=-1;
    if (snapshot.pointer_enabled) {
        struct mouse_event stale;
        while (mouse_scan(&stale)) {}
        console_write("trackpad-test: starting desktop; Escape returns to capture checks\n");
        char args[64]; unsigned arglen=0;
        append(args,&arglen,desktop_io?"--io-test ":snapshot.gestures_enabled?"--gesture-test":"--trackpad-test");
        if (desktop_io) append(args,&arglen,run);
        desktop_exit=process_run_elf_args("/fat/bin/deskd",args)?0:-1;
        while (keyboard_scan_char(&c)) {}
        xhci_trackpad_snapshot(&snapshot);
    }
    screen(run,&snapshot,input);
    console_write("trackpad-test: ready for gestures\n");
    uint64_t redraw=timer_ticks();
    for (;;) {
        bool finish=false,changed=false;
        /* This screen owns input after the compositor exits. Discard pointer
         * events while observing driver counters, so a long manual check does
         * not fill the bounded desktop queue. */
        struct mouse_event ignored;
        for (unsigned i=0;i<64 && mouse_scan(&ignored);i++) {}
        for (unsigned i=0;i<32 && keyboard_scan_char(&c);i++) {
            if (c=='\n' || c=='\r') { finish=true; break; }
            if (c==27) { count=0; input[0]=0; changed=true; }
            else if ((c==8 || c==127) && count) { input[--count]=0; changed=true; }
            else if (c>=32 && c<=126 && count<sizeof(input)-1) { input[count++]=c; input[count]=0; changed=true; }
        }
        if (finish) break;
        if (changed || timer_ticks()-redraw>=(snapshot.gestures_enabled?10u:100u)) {
            xhci_trackpad_snapshot(&snapshot); screen(run,&snapshot,input); redraw=timer_ticks();
        }
        timer_wait_ticks(1);
    }
    xhci_trackpad_snapshot(&snapshot);
    dropped=keyboard_dropped_count()-dropped;
    build_report(run,release,&snapshot,input,before,dropped,timer_monotonic_us()-start,desktop_exit);
    bool saved=!overflow && exfat_write_file(result_path,(const uint8_t *)report,used) &&
        workspace_sync() && file_equals(result_path,report,used);
    if (saved) {
        uint32_t crc=crc32((const uint8_t *)report,used);
        n=0; append(request,&n,"COMPLETE "); append(request,&n,run); append(request,&n," ");
        for (int i=7;i>=0;i--) request[n++]="0123456789abcdef"[(crc>>(i*4))&15];
        request[n++]='\n'; request[n]=0;
        saved=exfat_write_file(done_path,(const uint8_t *)request,n) && workspace_sync() && file_equals(done_path,request,n);
    }
    bool unmounted=workspace_unmount();
    console_clear();
    console_printf("trackpad-test: run=%s\n",run);
    bool keyboard=equal(input,"AaZz09") && snapshot.keyboard_reports>before && !dropped;
    console_printf("Capture coverage: %s\nKeyboard: %s\n",coverage(&snapshot,keyboard)?"COMPLETE":"INCOMPLETE",keyboard?"PASS":"INCOMPLETE");
    console_printf("Reports=%u touches=%u max fingers=%u clicks=%u lifts=%u\n",snapshot.reports,
        snapshot.contact_reports,(uint64_t)snapshot.max_contacts,snapshot.button_reports,snapshot.lifts);
    console_printf("Malformed=%u errors=%u\n",snapshot.malformed,snapshot.errors);
    if (snapshot.pointer_enabled) console_printf("Desktop exit=%s motion events=%u button edges=%u cancels=%u\n",
        desktop_exit==0?"PASS":"FAIL",snapshot.pointer_motion,snapshot.pointer_buttons,snapshot.pointer_cancels);
    if (snapshot.gestures_enabled) console_printf("Scroll events=%u typing-filter frames=%u broad-contact frames=%u\n",
        snapshot.scroll_events,snapshot.typing_suppressed,snapshot.palm_suppressed);
    console_printf("Report: %s\nUSB unmounted: %s\n",saved?result_path:"SAVE_FAILED",unmounted?"yes":"NO");
    if (desktop_io) console_write("USB workload: Ubuntu must verify its result, input counts and saved payload.\n");
    console_write("Host must verify the saved report; this is not gesture acceptance.\n");
}
void trackpad_test_boot(const char *cmdline) {
    char run[17],release[17];
    int active=option(cmdline,"srvros.trackpad-run=",run);
    if (!active) return;
    if (active<0 || option(cmdline,"srvros.trackpad-release=",release)!=1)
        console_write("trackpad-test: invalid run identity\n");
    else {
        char io_run[17]; int io=option(cmdline,"srvros.desktop-io=",io_run);
        if (io<0 || (io && !equal(run,io_run))) console_write("trackpad-test: invalid desktop I/O identity\n");
        else capture(run,release,io==1);
    }
    console_write("trackpad-test: HALTED - photograph if needed; restart manually\n");
    console_sync();
#ifndef SRVROS_TRACKPAD_TEST_HOST
    __asm__ volatile("cli" ::: "memory");
#endif
    halt_forever();
}
