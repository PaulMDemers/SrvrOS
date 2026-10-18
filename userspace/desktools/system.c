#include "app.h"
#include <srvros/desk_log.h>
#include <stdio.h>
#include <string.h>
static const struct desk_table_column columns[]={
    {"Process",0,0,1},{"PID",64,1,1},{"Parent",88,1,1},{"Mode",112,0,1},{"Mapped",108,1,1}
};
static int compare(const struct srv_process_info *a,const struct srv_process_info *b,int col) {
    int result=0;
    if(col==0) result=strcmp(a->name,b->name);
    else if(col==3) result=strcmp(a->state,b->state);
    else {
        uint64_t left=col==1?a->pid:col==2?a->parent_pid:a->user_mapped_bytes;
        uint64_t right=col==1?b->pid:col==2?b->parent_pid:b->user_mapped_bytes;
        result=left>right?1:left<right?-1:0;
    }
    if(!result) result=a->pid>b->pid?1:a->pid<b->pid?-1:0;
    return result;
}
static void rows(struct tool *t) {
    int col=t->table.sort_column<0?1:t->table.sort_column;
    for(int i=1;i<t->process_count;i++) {
        struct srv_process_info value=t->processes[i];int j=i;
        while(j>0 && compare(&t->processes[j-1],&value,col)*(t->table.descending?-1:1)>0) {
            t->processes[j]=t->processes[j-1];j--;
        }
        t->processes[j]=value;
    }
    for(int i=0;i<t->process_count;i++) {
        struct srv_process_info *p=&t->processes[i];
        snprintf(t->process_numbers[i][0],24,"%llu",(unsigned long long)p->pid);
        snprintf(t->process_numbers[i][1],24,"%llu",(unsigned long long)p->parent_pid);
        snprintf(t->process_numbers[i][2],24,"%llu KiB",(unsigned long long)(p->user_mapped_bytes/1024));
        t->table_rows[i]=(struct desk_table_row){.id=p->pid,.cells={p->name,t->process_numbers[i][0],t->process_numbers[i][1],p->state,t->process_numbers[i][2]}};
    }
    desk_table_rows(&t->table,t->table_rows,t->process_count);t->window.dirty=1;
}
static void ip_string(char *out,size_t size,uint32_t ip) {
    if(!ip) snprintf(out,size,"Not assigned");
    else snprintf(out,size,"%u.%u.%u.%u",ip>>24,(ip>>16)&255,(ip>>8)&255,ip&255);
}
static const char *network_state(const struct tool *t) {
    if(!t->network_valid) return "Network information unavailable";
    if(!t->network.initialized) return "No supported network adapter";
    if((t->network.link_flags&SRV_NET_LINK_KNOWN) && !(t->network.link_flags&SRV_NET_LINK_UP)) return "Network link disconnected";
    if(!t->network.worker_started) return "Network worker is stopped";
    return t->network.local_ip?"IPv4 address configured":"Waiting for an IPv4 address";
}
static void network_refresh(struct tool *t,int force) {
    struct srv_net_status_info n={0};
    t->network_valid=srv_net_status_info(&n)==0 && n.abi_version==SRV_NET_ABI_VERSION && n.struct_size>=sizeof(n);
    t->network=t->network_valid?n:(struct srv_net_status_info){0};
    static const char *labels[]={"IPv4 address","Router","DNS server","Hardware address","Address source",
        "Received frames","Sent frames","TCP connections","TCP listeners","UDP sockets","Pending connections","TCP table drops","Link","Link speed","Duplex"};
    unsigned count=sizeof(labels)/sizeof(*labels);
    for(unsigned i=0;i<count;i++) {
        snprintf(t->network_values[i],sizeof(t->network_values[i]),"—");
        t->network_rows[i]=(struct desk_table_row){.id=i+1,.cells={labels[i],t->network_values[i]}};
    }
    if(t->network_valid && n.initialized) {
        ip_string(t->network_values[0],80,n.local_ip);ip_string(t->network_values[1],80,n.router_ip);ip_string(t->network_values[2],80,n.dns_ip);
        snprintf(t->network_values[3],80,"%02x:%02x:%02x:%02x:%02x:%02x",n.local_mac[0],n.local_mac[1],n.local_mac[2],n.local_mac[3],n.local_mac[4],n.local_mac[5]);
        snprintf(t->network_values[4],80,"%s",n.dhcp_configured?"DHCP":n.local_ip?"Static / default":"Not configured");
        uint64_t numbers[]={n.rx_frames,n.tx_frames,n.tcp_connection_count,n.listener_count,n.udp_socket_count,n.pending_count,n.tcp_table_full_drops};
        for(unsigned i=0;i<7;i++) snprintf(t->network_values[i+5],80,"%llu",(unsigned long long)numbers[i]);
        int known=(n.link_flags&SRV_NET_LINK_KNOWN)!=0,up=(n.link_flags&SRV_NET_LINK_UP)!=0;
        snprintf(t->network_values[12],80,"%s",known?(up?"Connected":"Disconnected"):"Not available");
        if(known && up && n.link_speed_mbps) snprintf(t->network_values[13],80,"%llu Mb/s",(unsigned long long)n.link_speed_mbps);
        snprintf(t->network_values[14],80,"%s",known && up?(n.link_flags&SRV_NET_LINK_FULL_DUPLEX?"Full duplex":"Half duplex"):"—");
    }
    desk_table_rows(&t->network_table,t->network_rows,count);
    if(force) desk_log("desktools: network-refresh valid=%d initialized=%llu ip=%u rx=%llu tx=%llu paused=%d link=%llu speed=%llu\n",t->network_valid,
        (unsigned long long)t->network.initialized,t->network.local_ip,(unsigned long long)t->network.rx_frames,(unsigned long long)t->network.tx_frames,t->system_paused,(unsigned long long)t->network.link_flags,(unsigned long long)t->network.link_speed_mbps);
}
void tool_system_refresh(struct tool *t,int force) {
    uint64_t now=srv_ticks();if(!force && (t->system_paused || now-t->refresh_tick<100)) return;t->refresh_tick=now;
    long index=0;int count=0;struct srv_process_info p;
    while(count<DESK_TABLE_MAX_ROWS && (index=srv_proc_list((uint64_t)index,&p))>0) {
        p.name[sizeof(p.name)-1]=0;p.state[sizeof(p.state)-1]=0;t->processes[count++]=p;
    }
    if(force) desk_log("desktools: system-refresh processes=%d paused=%d\n",count,t->system_paused);
    t->process_count=count;t->system_memory_valid=srv_meminfo(&t->system_memory)==0;rows(t);network_refresh(t,force);
    if(desk_tabs_selected(&t->system_tabs)==SYSTEM_NETWORK) snprintf(t->status,sizeof(t->status),"%s · %s",t->system_paused?"Paused":"Updates every second","F5 refreshes");
    else if(index<0) tool_status(t,"Process list unavailable. Press Refresh to try again.");
    else snprintf(t->status,sizeof(t->status),"%d processes%s · F6 selects headings · F5 refreshes",count,t->system_paused?" · Paused":"");
}
void tool_system_init(struct tool *t) {
    desk_tabs_init(&t->system_tabs);
    const struct desk_tab tabs[]={{SYSTEM_PROCESSES,"Processes",0},{SYSTEM_NETWORK,"Network",0}};
    desk_tabs_items(&t->system_tabs,tabs,2);
    desk_table_init(&t->network_table);
    static const struct desk_table_column network_columns[]={{"Property",0,0,0},{"Value",0,0,0}};
    desk_table_columns(&t->network_table,network_columns,2);
    desk_ui_add(&t->ui,SYSTEM_TABS,DESK_CONTROL_TABS,&t->system_tabs,NULL);
    desk_ui_add(&t->ui,SYSTEM_NETWORK_TABLE,DESK_CONTROL_TABLE,&t->network_table,NULL);
    desk_table_init(&t->table);desk_table_columns(&t->table,columns,5);desk_table_sort_indicator(&t->table,1,0);
    desk_ui_add(&t->ui,SYSTEM_TABLE,DESK_CONTROL_TABLE,&t->table,NULL);
    desk_ui_add(&t->ui,SYSTEM_PAUSE,DESK_CONTROL_BUTTON,&t->buttons[0],"Pause");
    desk_ui_add(&t->ui,SYSTEM_REFRESH,DESK_CONTROL_BUTTON,&t->buttons[1],"Refresh");
    desk_ui_add(&t->ui,SYSTEM_DIAGNOSTICS,DESK_CONTROL_BUTTON,&t->buttons[2],"Diagnostics");
    tool_system_refresh(t,1);desk_log("desktools: system-ready processes=%d total=%llu\n",t->process_count,(unsigned long long)t->system_memory.total_bytes);
}
void tool_system_layout(struct tool *t) {
    int w=t->window.width,h=t->window.height;
    desk_button_configure(&t->buttons[0],(struct desk_rect){w-196,18,80,28},1);
    desk_button_configure(&t->buttons[1],(struct desk_rect){w-104,18,88,28},1);
    desk_button_configure(&t->buttons[2],w>=500?(struct desk_rect){w-324,18,116,28}:(struct desk_rect){0},w>=500);
    desk_tabs_configure(&t->system_tabs,(struct desk_rect){16,56,w-32,32},1);
    int network=desk_tabs_selected(&t->system_tabs)==SYSTEM_NETWORK;
    desk_table_configure(&t->table,network?(struct desk_rect){0}:(struct desk_rect){16,158,w-32,h>232?h-232:0},!network);
    desk_table_configure(&t->network_table,network?(struct desk_rect){16,132,w-32,h>176?h-176:0}:(struct desk_rect){0},network);
}
void tool_system_action(struct tool *t,struct desk_action a) {
    if(a.id==SYSTEM_TABLE && (a.flags&DESK_TABLE_SORT)) {
        rows(t);desk_log("desktools: system-sort column=%d descending=%d\n",t->table.sort_column,t->table.descending);
    }
    if(!(a.flags&DESK_WIDGET_ACTIVATED)) return;
    if(a.id==SYSTEM_PROCESSES || a.id==SYSTEM_NETWORK) { desk_tabs_select(&t->system_tabs,a.id);a.id=SYSTEM_TABS; }
    if(a.id==SYSTEM_TABS) {
        tool_system_layout(t);desk_ui_layout_changed(&t->ui);tool_system_refresh(t,1);
        desk_log("desktools: system-page=%s\n",desk_tabs_selected(&t->system_tabs)==SYSTEM_NETWORK?"network":"processes");
    }
    if(a.id==SYSTEM_DIAGNOSTICS) tool_launch(t,DESK_APP_NETWORK,"");
    if(a.id==SYSTEM_REFRESH) tool_system_refresh(t,1);
    if(a.id==SYSTEM_PAUSE) {
        t->system_paused=!t->system_paused;
        for(unsigned i=0;i<t->ui.count;i++) if(t->ui.controls[i].id==SYSTEM_PAUSE) t->ui.controls[i].label=t->system_paused?"Resume":"Pause";
        tool_system_refresh(t,1);desk_log("desktools: system-paused=%d\n",t->system_paused);
    }
}
void tool_system_draw(struct tool *t,struct desk_canvas *c) {
    const struct desk_theme *theme=&desk_theme_quiet;
    desk_label_draw(c,(struct desk_rect){16,18,c->width-(c->width>=500?352:224),30},theme->heading,"System",theme->text);
    if(desk_tabs_selected(&t->system_tabs)==SYSTEM_NETWORK) {
        desk_label_draw(c,(struct desk_rect){16,98,c->width-32,26},theme->body,network_state(t),theme->text);return;
    }
    char label[128];
    if(t->system_memory_valid) {
        struct srv_meminfo *m=&t->system_memory;
        snprintf(label,sizeof(label),"Memory: %llu MiB used · %llu MiB free · %llu MiB RAM",
            (unsigned long long)(m->used_bytes/1048576),(unsigned long long)(m->free_bytes/1048576),(unsigned long long)(m->total_bytes/1048576));
    } else strcpy(label,"Memory information unavailable");
    desk_label_draw(c,(struct desk_rect){16,98,c->width-32,26},theme->body,label,theme->text);
    struct desk_rect bar={16,134,c->width-32,8};desk_fill(c,bar,theme->border);
    if(t->system_memory_valid && t->system_memory.total_bytes && t->system_memory.used_bytes<=t->system_memory.total_bytes) {
        bar.w=(int)((uint64_t)bar.w*t->system_memory.used_bytes/t->system_memory.total_bytes);desk_fill(c,bar,theme->accent);
    }
    desk_label_draw(c,(struct desk_rect){16,c->height-64,c->width-32,24},theme->body,"Mapped = private user pages; shared surfaces and kernel memory excluded.",theme->muted);
}
