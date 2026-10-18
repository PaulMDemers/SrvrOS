#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/desktools/system.c"
static uint64_t ticks=500;
static int phase,reads,net_phase,net_reads;
long srv_ticks(void) { return ticks; }
void srv_puts(const char *s) { (void)s; }
long srv_proc_list(uint64_t index,struct srv_process_info *p) {
    reads++;
    if(phase==2) return -1;
    if(index>=(phase?2u:3u)) return 0;
    unsigned ids[]={9,2,33};const char *names[]={"zeta","Alpha","beta"};
    *p=(struct srv_process_info){.pid=ids[index],.parent_pid=1,.user_mapped_bytes=(3-index)*4096};
    strcpy(p->name,names[index]);strcpy(p->state,index?"background":"foreground");return index+1;
}
long srv_meminfo(struct srv_meminfo *m) { *m=(struct srv_meminfo){.total_bytes=512*1048576,.free_bytes=300*1048576,.used_bytes=212*1048576};return 0; }
long srv_net_status_info(struct srv_net_status_info *n) {
    net_reads++;
    if(net_phase==1) return -1;
    *n=(struct srv_net_status_info){.abi_version=SRV_NET_ABI_VERSION,.struct_size=sizeof(*n),
        .initialized=net_phase!=2,.worker_started=1,.dhcp_configured=1,.local_ip=0x0a00020f,.router_ip=0x0a000202,.dns_ip=0x0a000203,
        .rx_frames=123,.tx_frames=456,.tcp_connection_count=2,.listener_count=3,.udp_socket_count=4,.local_mac={0x52,0x54,0,0x12,0x34,0x56}};
    if(net_phase==3) n->struct_size=16;
    if(net_phase>=4) n->link_flags=SRV_NET_LINK_KNOWN;
    if(net_phase==5) { n->link_flags|=SRV_NET_LINK_UP|SRV_NET_LINK_FULL_DUPLEX;n->link_speed_mbps=1000; }
    return 0;
}
void tool_status(struct tool *t,const char *text) { snprintf(t->status,sizeof(t->status),"%s",text);t->window.dirty=1; }
void tool_launch(struct tool *t,int application,const char *path) { (void)t;assert(application==DESK_APP_NETWORK && !*path); }
static struct tool t;
int main(void) {
    t.window.width=760;t.window.height=500;desk_ui_init(&t.ui);tool_system_init(&t);tool_system_layout(&t);desk_ui_layout_changed(&t.ui);
    assert(t.process_count==3 && t.processes[0].pid==2 && t.processes[2].pid==33);
    desk_list_select(&t.table.body,1);assert(t.table.navigation[1].id==9);
    desk_table_sort_indicator(&t.table,4,1);tool_system_action(&t,(struct desk_action){SYSTEM_TABLE,DESK_TABLE_SORT});
    assert(t.processes[0].pid==9 && t.table.body.selected==0);
    int before=reads;tool_system_refresh(&t,0);assert(reads==before);
    ticks+=100;phase=1;tool_system_refresh(&t,0);assert(t.process_count==2 && t.table.body.selected==0);
    tool_system_action(&t,(struct desk_action){SYSTEM_PAUSE,DESK_WIDGET_ACTIVATED});assert(t.system_paused);
    before=reads;ticks+=200;tool_system_refresh(&t,0);assert(reads==before);
    tool_system_action(&t,(struct desk_action){SYSTEM_REFRESH,DESK_WIDGET_ACTIVATED});assert(reads>before);
    phase=2;tool_system_refresh(&t,1);assert(strstr(t.status,"unavailable") && t.process_count==0 && t.table.body.selected==-1);
    phase=0;tool_system_action(&t,(struct desk_action){SYSTEM_PAUSE,DESK_WIDGET_ACTIVATED});assert(!t.system_paused && t.process_count==3);
    assert(t.network_valid && !strcmp(t.network_values[0],"10.0.2.15") && !strcmp(t.network_values[3],"52:54:00:12:34:56"));
    assert(!strcmp(t.network_values[5],"123") && !strcmp(t.network_values[8],"3"));
    desk_list_select(&t.table.body,1);uint64_t selected=t.table.navigation[t.table.body.selected].id;
    tool_system_action(&t,(struct desk_action){SYSTEM_NETWORK,DESK_WIDGET_ACTIVATED});
    assert(desk_tabs_selected(&t.system_tabs)==SYSTEM_NETWORK && !t.table.body.enabled && t.network_table.body.enabled);
    assert(strstr(t.status,"Updates") && !strcmp(network_state(&t),"IPv4 address configured"));
    tool_system_action(&t,(struct desk_action){SYSTEM_PAUSE,DESK_WIDGET_ACTIVATED});before=net_reads;ticks+=200;tool_system_refresh(&t,0);assert(net_reads==before);
    tool_system_action(&t,(struct desk_action){SYSTEM_REFRESH,DESK_WIDGET_ACTIVATED});assert(net_reads>before);
    net_phase=1;tool_system_refresh(&t,1);assert(!t.network_valid && strstr(network_state(&t),"unavailable") && !strcmp(t.network_values[0],"—"));
    net_phase=2;tool_system_refresh(&t,1);assert(t.network_valid && strstr(network_state(&t),"No supported") && !strcmp(t.network_values[5],"—"));
    net_phase=3;tool_system_refresh(&t,1);assert(!t.network_valid);
    net_phase=4;tool_system_refresh(&t,1);assert(strstr(network_state(&t),"disconnected") && !strcmp(t.network_values[12],"Disconnected") && !strcmp(t.network_values[13],"—"));
    net_phase=5;tool_system_refresh(&t,1);assert(!strcmp(t.network_values[12],"Connected") && !strcmp(t.network_values[13],"1000 Mb/s") && !strcmp(t.network_values[14],"Full duplex"));
    net_phase=0;tool_system_action(&t,(struct desk_action){SYSTEM_PROCESSES,DESK_WIDGET_ACTIVATED});assert(t.table.body.enabled && !t.network_table.body.enabled);
    assert(t.table.navigation[t.table.body.selected].id==selected);
    static uint32_t pixels[760*500];struct desk_canvas c;assert(!desk_canvas_init(&c,pixels,760,500,760));
    struct desk_rect clip={20,50,700,400};desk_clip(&c,clip);desk_ui_draw(&c,&t.ui,&desk_theme_quiet);tool_system_draw(&t,&c);
    assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    for(int y=0;y<500;y++) for(int x=0;x<760;x++) if(x<20 || x>=720 || y<50 || y>=450) assert(!pixels[y*760+x]);
    puts("system: PASS process sorting, stable selection, refresh cadence, pause/manual refresh, errors, network ABI/state/counters, page selection preservation and clipped rendering");
}
