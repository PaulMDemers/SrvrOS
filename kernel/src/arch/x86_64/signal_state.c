#include "signal_state.h"

static bool user_address(uint64_t p) { return p && p < (UINT64_C(1)<<47); }
static bool valid_identity(struct signal_state_identity i) {
    return i.image_generation && i.public_tid && i.public_tid<=INT64_MAX && i.scheduler_identity;
}
static bool same_identity(struct signal_state_identity a,struct signal_state_identity b) {
    return a.image_generation==b.image_generation && a.public_tid==b.public_tid &&
        a.scheduler_identity==b.scheduler_identity;
}
bool signal_action_descriptor_valid(const struct signal_action_descriptor *a) {
    if (!a || a->mode>SIGNAL_ACTION_INFO || (a->flags & ~(uint64_t)(SIGNAL_ACTION_RESTART|SIGNAL_ACTION_RESET)) ||
        (a->mask & (UINT64_C(1)|(UINT64_C(1)<<9)|(UINT64_C(1)<<19)))) return false;
    switch (a->mode) {
    case SIGNAL_ACTION_DEFAULT: case SIGNAL_ACTION_IGNORE: return !a->handler && !a->trampoline;
    case SIGNAL_ACTION_LEGACY: return !a->trampoline && (!a->handler || (a->handler>1 && user_address(a->handler)));
    case SIGNAL_ACTION_INFO: return a->handler>1 && a->trampoline>1 &&
        user_address(a->handler) && user_address(a->trampoline);
    default: return false;
    }
}
void signal_thread_state_reset(struct signal_thread_state *state) {
    if (state) *state=(struct signal_thread_state){0};
}
static bool all_zero(const void *p,size_t n) {
    const unsigned char *b=p;for(size_t i=0;i<n;i++) if(b[i]) return false;return true;
}
static bool runtime_idle_and_io_valid(const struct signal_thread_state *s,bool allow_inflight) {
    if(s->request_reserved || s->request_sequence>INT64_MAX || s->io_sequence>INT64_MAX) return false;
    const struct signal_request_slot *r=&s->request;
    if(!r->sequence) {if(!all_zero(r,sizeof(*r)))return false;}
    else if(!same_identity(r->identity,s->identity) || r->sequence>s->request_sequence || !r->trap ||
        !r->operation || r->kind<SIGNAL_REQUEST_CONTROL || r->kind>SIGNAL_REQUEST_IO ||
        (r->completion_flags&~SIGNAL_REQUEST_COMPLETION_MASK) || r->output_size>1024 ||
        (!!r->output_address != !!r->output_size) || (r->output_address && (!user_address(r->output_address) ||
        r->output_size>(UINT64_C(1)<<47)-r->output_address))) return false;
    for(unsigned i=0;i<4;i++) {
        const struct signal_wait_slot *w=&s->waits[i];
        if(!w->ticket) {if(!all_zero(w,sizeof(*w)))return false;continue;}
        uint64_t phase=w->phase_flags&255;
        if(w->ticket>s->io_sequence || !user_address(w->anchor) || !w->request_sequence ||
           w->request_sequence>s->request_sequence || w->reserved || i>s->stack.depth ||
           ((w->previous_mask|w->temporary_mask)&((UINT64_C(1)<<9)|(UINT64_C(1)<<19))) ||
           (w->phase_flags&~(UINT64_C(255)|SIGNAL_WAIT_INFO_ONLY|SIGNAL_WAIT_INFO_COMPLETED)) ||
           phase<SIGNAL_WAIT_PARKED || phase>SIGNAL_WAIT_INFO_DONE ||
           (phase==SIGNAL_WAIT_HANDLER ? !w->record_token : !!w->record_token) ||
           ((w->phase_flags&SIGNAL_WAIT_INFO_COMPLETED) && phase!=SIGNAL_WAIT_LEGACY_READY && phase!=SIGNAL_WAIT_HANDLER)) return false;
        for(unsigned j=0;j<i;j++) if(s->waits[j].ticket==w->ticket) return false;
        for(unsigned j=0;j<4;j++) if((s->io[j].flags&SIGNAL_IO_LIVE) &&
            (s->io[j].ticket==w->ticket || s->io[j].cookie==w->ticket)) return false;
        if(s->local.cookie && s->local.cookie==w->ticket) return false;
        if(phase==SIGNAL_WAIT_HANDLER) {
            if(i>=s->stack.depth) return false;
            const struct signal_context_record *record=&s->stack.records[i];
            if(record->token!=w->record_token || record->wait_ticket!=w->ticket ||
               (record->wait_mode!=SIGNAL_WAIT_MASK_OVERRIDE && record->wait_mode!=SIGNAL_WAIT_COMPLETION_ONLY) ||
               (record->wait_mode==SIGNAL_WAIT_MASK_OVERRIDE && (record->mask!=w->previous_mask || (w->phase_flags&SIGNAL_WAIT_INFO_COMPLETED)))) return false;
        }
    }
    for(unsigned i=s->stack.depth;i<SIGNAL_CONTEXT_MAX_DEPTH;i++)
        if(s->stack.records[i].wait_ticket || s->stack.records[i].wait_mode) return false;
    for(unsigned i=0;i<s->stack.depth;i++) {
        const struct signal_context_record *record=&s->stack.records[i];
        if(record->wait_ticket || record->wait_mode) {
            if(!record->wait_ticket || !record->wait_mode || s->waits[i].ticket!=record->wait_ticket ||
                (s->waits[i].phase_flags&255)!=SIGNAL_WAIT_HANDLER) return false;
        }
    }
    unsigned count=0,inflight=0;const uint64_t allowed=UINT64_C(0x00ffffffffffffff);
    for(unsigned i=0;i<4;i++) {
        const struct signal_io_slot *v=&s->io[i];
        if(!(v->flags&SIGNAL_IO_LIVE)) {if(!all_zero(v,sizeof(*v)))return false;continue;}
        count++;
        if(v->flags&SIGNAL_IO_IN_FLIGHT) {
            if(!allow_inflight || ++inflight>1 || v->ticket!=s->io_top || !v->cookie || s->local.flags) return false;
        }
        uint64_t kind=v->flags&255;
        if((v->flags&~allowed) || kind<1 || kind>8 || !v->ticket || v->ticket>s->io_sequence ||
           v->depth>s->stack.depth || !user_address(v->anchor) ||
           (!(v->flags&SIGNAL_IO_LEGACY_MASKED) && (v->previous_mask||v->temporary_mask)) ||
           ((v->flags&SIGNAL_IO_FINALIZED) && !(v->flags&SIGNAL_IO_COMPLETED)) ||
           ((v->flags&SIGNAL_IO_IN_FLIGHT) && (v->flags&SIGNAL_IO_COMPLETED)) ||
           (v->cookie && (v->cookie<=v->ticket || v->cookie>s->io_sequence)) ||
           (!v->cookie && (v->result || v->elapsed_us || v->elapsed_ticks ||
               (v->flags&(SIGNAL_IO_COMPLETED|SIGNAL_IO_FINALIZED|SIGNAL_IO_MONO_VALID|SIGNAL_IO_TICKS_VALID)) ||
               ((v->flags>>32)&UINT16_MAX))) ||
           (!(v->flags&SIGNAL_IO_MONO_VALID) && v->elapsed_us) ||
           (!(v->flags&SIGNAL_IO_TICKS_VALID) && v->elapsed_ticks)) return false;
        for(unsigned j=0;j<i;j++) if(s->io[j].flags&SIGNAL_IO_LIVE)
            if(s->io[j].ticket==v->ticket || (v->cookie && s->io[j].cookie==v->cookie) ||
               s->io[j].ticket==v->cookie || (s->io[j].cookie && s->io[j].cookie==v->ticket)) return false;
    }
    uint64_t next=s->io_top,last_ticket=UINT64_MAX,last_anchor=0,last_depth=UINT64_MAX;
    unsigned traversed=0;
    while(next) {
        const struct signal_io_slot *v=NULL;
        for(unsigned i=0;i<4;i++) if((s->io[i].flags&SIGNAL_IO_LIVE) && s->io[i].ticket==next) v=&s->io[i];
        if(!v || ++traversed>count || v->ticket>=last_ticket || v->anchor<=last_anchor || v->depth>last_depth) return false;
        last_ticket=v->ticket;last_anchor=v->anchor;last_depth=v->depth;next=v->parent;
    }
    if(traversed!=count) return false;
    if(!s->local.flags) return all_zero(&s->local,sizeof(s->local));
    if((s->local.flags&~(SIGNAL_LOCAL_ACTIVE|SIGNAL_LOCAL_NO_MASK)) ||
       !(s->local.flags&SIGNAL_LOCAL_ACTIVE) || s->local.owner_ticket!=s->io_top ||
       !s->local.cookie || s->local.cookie>s->io_sequence) return false;
    const struct signal_io_slot *v=NULL;
    for(unsigned i=0;i<4;i++) if(s->io[i].ticket==s->io_top && (s->io[i].flags&SIGNAL_IO_LIVE))v=&s->io[i];
    if(!v || v->depth!=s->stack.depth || s->local.cookie<=v->ticket ||
       !!(s->local.flags&SIGNAL_LOCAL_NO_MASK)!=!!(v->flags&SIGNAL_IO_LEGACY_MASKED)) return false;
    for(unsigned i=0;i<4;i++) if((s->io[i].flags&SIGNAL_IO_LIVE) &&
       (s->io[i].ticket==s->local.cookie || s->io[i].cookie==s->local.cookie)) return false;
    return true;
}
static bool valid_bound_mode(const struct signal_thread_state *state,struct signal_state_identity id,bool allow_inflight) {
    if (!state || !valid_identity(id) || !same_identity(state->identity,id) ||
        state->stack.depth>SIGNAL_CONTEXT_MAX_DEPTH || !runtime_idle_and_io_valid(state,allow_inflight)) return false;
    uint64_t previous=0;
    for (unsigned i=0;i<state->stack.depth;i++) {
        const struct signal_context_record *r=&state->stack.records[i];
        struct signal_context_range range=state->ranges[i];
        struct signal_context_snapshot check;
        if (r->image_generation!=id.image_generation || r->thread_generation!=id.scheduler_identity ||
            !r->token || r->token<=previous || r->token>state->stack.last_token ||
            !user_address(range.start) || range.end<=range.start || !user_address(range.end-1) ||
            range.end-range.start>SIGNAL_CONTEXT_MAX_FRAME || r->frame_identity!=range.start ||
            !signal_context_capture(&check,&r->frame,r->fpu,0,r->token,r->mask)) return false;
        for (unsigned j=0;j<i;j++)
            if (range.start<state->ranges[j].end && state->ranges[j].start<range.end) return false;
        previous=r->token;
    }
    return true;
}
static bool valid_bound(const struct signal_thread_state *s,struct signal_state_identity id) {
    return valid_bound_mode(s,id,false);
}
bool signal_state_validate(const struct signal_thread_state *s,struct signal_state_identity id) {
    return valid_bound(s,id);
}
bool signal_thread_state_bind(struct signal_thread_state *state,struct signal_state_identity id) {
    if (!state || !valid_identity(id) || state->stack.depth>SIGNAL_CONTEXT_MAX_DEPTH) return false;
    if (same_identity(state->identity,id)) return valid_bound(state,id);
    if (state->identity.image_generation || state->identity.public_tid || state->identity.scheduler_identity ||
        state->stack.depth || state->stack.last_token ||
        !all_zero(state->waits,sizeof(state->waits)) || !all_zero(&state->request,sizeof(state->request)) ||
        state->request_sequence || state->request_reserved || !all_zero(state->io,sizeof(state->io)) ||
        state->io_sequence || state->io_top || !all_zero(&state->local,sizeof(state->local))) return false;
    for(unsigned i=0;i<SIGNAL_CONTEXT_MAX_DEPTH;i++)
        if(state->stack.records[i].wait_ticket || state->stack.records[i].wait_mode) return false;
    state->identity=id;
    return true;
}

bool signal_thread_state_push(struct signal_thread_state *state,struct signal_state_identity id,
    const struct signal_context_record *record,struct signal_context_range range) {
    if (!valid_bound(state,id) || state->local.flags || state->request.sequence || !record || record->wait_ticket || record->wait_mode || state->stack.depth==SIGNAL_CONTEXT_MAX_DEPTH ||
        !user_address(range.start) || range.end<=range.start || !user_address(range.end-1) ||
        range.end-range.start>SIGNAL_CONTEXT_MAX_FRAME || record->frame_identity!=range.start) return false;
    for (unsigned i=0;i<state->stack.depth;i++)
        if (range.start<state->ranges[i].end && state->ranges[i].start<range.end) return false;
    struct signal_thread_state next=*state;
    if (!signal_context_push(&next.stack,record,id.image_generation,id.scheduler_identity)) return false;
    next.ranges[next.stack.depth-1]=range;
    if(!valid_bound(&next,id)) return false;
    *state=next;
    return true;
}
bool signal_thread_state_pop(struct signal_thread_state *state,struct signal_state_identity id,
    uint64_t token,uint64_t frame_identity,struct signal_context_record *record_out,
    struct signal_context_range *range_out) {
    if (!record_out || !range_out || !valid_bound(state,id) || !state->stack.depth) return false;
    for(unsigned i=0;i<4;i++) if((state->io[i].flags&SIGNAL_IO_LIVE) && state->io[i].depth==state->stack.depth) return false;
    if(state->local.flags || state->stack.records[state->stack.depth-1].wait_ticket ||
       state->stack.records[state->stack.depth-1].wait_mode) return false;
    struct signal_thread_state next=*state;
    unsigned index=next.stack.depth-1;
    struct signal_context_record record;
    struct signal_context_range range=next.ranges[index];
    if (!signal_context_pop(&next.stack,&record,id.image_generation,id.scheduler_identity,token,frame_identity)) return false;
    next.ranges[index]=(struct signal_context_range){0};
    *state=next;*record_out=record;*range_out=range;
    return true;
}

/* These helpers deliberately do not authenticate user addresses, select a
 * backend, modify a signal mask, or publish anything to userspace. */
_Static_assert(sizeof(struct signal_thread_state)==4032,"bounded selected state");
_Static_assert(sizeof(struct signal_io_slot)==96,"logical IO slot");
_Static_assert(sizeof(struct signal_wait_slot)==64,"wait slot");
_Static_assert(sizeof(struct signal_local_guard)==32,"local guard");
_Static_assert(sizeof(struct signal_context_record)==752,"context record with wait association");
_Static_assert(sizeof(struct signal_request_slot)==208,"request slot");
static struct signal_io_slot *io_top(struct signal_thread_state *s,struct signal_state_identity id,
    uint64_t depth,uint64_t ticket) {
    if (!valid_bound(s,id) || depth!=s->stack.depth || !ticket || s->io_top!=ticket) return NULL;
    struct signal_io_slot *found=NULL;
    for(unsigned i=0;i<4;i++) if(s->io[i].ticket==ticket && (s->io[i].flags&SIGNAL_IO_LIVE)) {
        if(found) return NULL;
        found=&s->io[i];
    }
    return found && found->depth==depth ? found : NULL;
}
bool signal_io_begin(struct signal_thread_state *s,struct signal_state_identity id,uint64_t depth,
    uint64_t kind,uint64_t primary,uint64_t selector,uint64_t anchor,uint64_t deadline,
    uint64_t flags,uint64_t previous_mask,uint64_t temporary_mask,uint64_t *out) {
    if(!out || !valid_bound(s,id) || depth!=s->stack.depth || kind<1 || kind>8 ||
       primary>UINT16_MAX || selector>UINT8_MAX || !user_address(anchor) ||
       (flags&~SIGNAL_IO_LEGACY_MASKED) || (!(flags&SIGNAL_IO_LEGACY_MASKED) && (previous_mask||temporary_mask)) ||
       s->local.flags || s->io_sequence>=INT64_MAX) return false;
    struct signal_io_slot *parent=NULL,*free_slot=NULL;
    for(unsigned i=0;i<4;i++) {
        struct signal_io_slot *slot=&s->io[i];
        if(!slot->flags && !slot->ticket && !free_slot) free_slot=slot;
        if(slot->ticket==s->io_top && (slot->flags&SIGNAL_IO_LIVE)) parent=slot;
    }
    if(!free_slot || (s->io_top && (!parent || anchor>=parent->anchor))) return false;
    uint64_t ticket=s->io_sequence+1;
    *free_slot=(struct signal_io_slot){.ticket=ticket,.parent=s->io_top,.depth=depth,
        .flags=SIGNAL_IO_LIVE|flags|kind|(primary<<8)|(selector<<24),.deadline=deadline,
        .anchor=anchor,.previous_mask=previous_mask,.temporary_mask=temporary_mask};
    s->io_sequence=ticket;s->io_top=ticket;*out=ticket;return true;
}
bool signal_io_admit(const struct signal_thread_state *state,struct signal_state_identity id,uint64_t depth,uint64_t ticket) {
    struct signal_thread_state *s=(struct signal_thread_state *)state;
    struct signal_io_slot *slot=io_top(s,id,depth,ticket);
    return slot && !s->local.flags && !(slot->flags&(SIGNAL_IO_INTERRUPTED|SIGNAL_IO_COMPLETED));
}
bool signal_io_prepare_outcome(struct signal_thread_state *s,struct signal_state_identity id,uint64_t depth,uint64_t ticket,uint64_t raw,uint64_t *out) {
    struct signal_io_slot *slot=io_top(s,id,depth,ticket);
    if(!out || !slot || !raw || raw>UINT16_MAX || s->local.flags || (slot->flags&(SIGNAL_IO_INTERRUPTED|SIGNAL_IO_COMPLETED)) || s->io_sequence>=INT64_MAX) return false;
    uint64_t cookie=s->io_sequence+1;
    slot->flags &= ~((UINT64_C(0xffff)<<32)|SIGNAL_IO_MONO_VALID|SIGNAL_IO_TICKS_VALID);
    slot->flags |= SIGNAL_IO_IN_FLIGHT | (raw<<32);
    slot->result=0;slot->elapsed_us=slot->elapsed_ticks=0;slot->cookie=cookie;
    s->io_sequence=cookie;*out=cookie;return true;
}
bool signal_io_commit_outcome(struct signal_thread_state *s,struct signal_state_identity id,uint64_t depth,uint64_t ticket,
    uint64_t cookie,uint64_t raw,int64_t result,uint64_t us,uint64_t ticks,bool mono,bool tickvalid,bool complete) {
    if(!valid_bound_mode(s,id,true) || !cookie || raw>UINT16_MAX || (!mono && us) || (!tickvalid && ticks) ||
       depth!=s->stack.depth || ticket!=s->io_top) return false;
    struct signal_io_slot *slot=NULL;
    for(unsigned i=0;i<4;i++) if((s->io[i].flags&SIGNAL_IO_LIVE) && s->io[i].ticket==ticket) slot=&s->io[i];
    if(!slot || slot->depth!=depth || !(slot->flags&SIGNAL_IO_IN_FLIGHT) || slot->cookie!=cookie || ((slot->flags>>32)&UINT16_MAX)!=raw) return false;
    slot->flags &= ~SIGNAL_IO_IN_FLIGHT;
    if(mono) slot->flags|=SIGNAL_IO_MONO_VALID;
    if(tickvalid) slot->flags|=SIGNAL_IO_TICKS_VALID;
    if(complete) slot->flags|=SIGNAL_IO_COMPLETED;
    slot->result=result;slot->elapsed_us=us;slot->elapsed_ticks=ticks;return true;
}
bool signal_io_mark_interrupted(struct signal_thread_state *s,struct signal_state_identity id) {
    if(!valid_bound(s,id) || s->local.flags) return false;
    for(unsigned i=0;i<4;i++) if(s->io[i].flags&SIGNAL_IO_LIVE) s->io[i].flags|=SIGNAL_IO_INTERRUPTED;
    return true;
}
bool signal_io_end(struct signal_thread_state *s,struct signal_state_identity id,uint64_t depth,uint64_t ticket) {
    struct signal_io_slot *slot=io_top(s,id,depth,ticket);
    if(!slot || s->local.flags) return false;
    uint64_t parent=slot->parent;*slot=(struct signal_io_slot){0};s->io_top=parent;return true;
}
bool signal_io_local_begin(struct signal_thread_state *s,struct signal_state_identity id,uint64_t depth,uint64_t ticket,
    uint64_t mask,bool finalize,uint64_t outcome,uint64_t *out) {
    struct signal_io_slot *slot=io_top(s,id,depth,ticket);
    if(!out || !slot || s->local.flags || s->io_sequence>=INT64_MAX) return false;
    if(finalize) {
        if(!(slot->flags&SIGNAL_IO_COMPLETED) || (slot->flags&SIGNAL_IO_FINALIZED) || !outcome || outcome!=slot->cookie) return false;
    } else if(outcome || (slot->flags&(SIGNAL_IO_INTERRUPTED|SIGNAL_IO_COMPLETED))) return false;
    uint64_t cookie=s->io_sequence+1;
    if(finalize)slot->flags|=SIGNAL_IO_FINALIZED;
    s->local=(struct signal_local_guard){ticket,cookie,mask,SIGNAL_LOCAL_ACTIVE|
        ((slot->flags&SIGNAL_IO_LEGACY_MASKED)?SIGNAL_LOCAL_NO_MASK:0)};
    s->io_sequence=cookie;*out=cookie;return true;
}
bool signal_io_local_end(struct signal_thread_state *s,struct signal_state_identity id,uint64_t depth,uint64_t ticket,
    uint64_t cookie,uint64_t *mask,bool *no_mask) {
    if(!mask || !no_mask || !io_top(s,id,depth,ticket) || !(s->local.flags&SIGNAL_LOCAL_ACTIVE) ||
       s->local.owner_ticket!=ticket || !cookie || s->local.cookie!=cookie) return false;
    uint64_t old=s->local.previous_mask;bool no=(s->local.flags&SIGNAL_LOCAL_NO_MASK)!=0;
    s->local=(struct signal_local_guard){0};*mask=old;*no_mask=no;return true;
}

bool signal_request_stage(struct signal_thread_state *s,struct signal_state_identity id,uint64_t trap,uint64_t operation,
    uint64_t output,uint64_t output_size,const uint8_t wire[128],uint64_t kind,uint64_t flags,uint64_t *out) {
    if(!wire || !out || !valid_bound(s,id) || s->request.sequence || s->local.flags ||
        s->request_sequence>=INT64_MAX || !trap || !operation || kind<SIGNAL_REQUEST_CONTROL || kind>SIGNAL_REQUEST_IO ||
        (flags&~SIGNAL_REQUEST_COMPLETION_MASK) || output_size>1024 || (!!output!=!!output_size) ||
        (output && (!user_address(output) || output_size>(UINT64_C(1)<<47)-output))) return false;
    struct signal_request_slot request={.identity=id,.sequence=s->request_sequence+1,.trap=trap,.operation=operation,
        .output_address=output,.output_size=output_size,.kind=kind,.completion_flags=flags};
    for(unsigned i=0;i<128;i++)request.wire[i]=wire[i];
    s->request=request;s->request_sequence=request.sequence;*out=request.sequence;return true;
}
bool signal_request_clear(struct signal_thread_state *s,struct signal_state_identity id,uint64_t trap,uint64_t seq) {
    if(!valid_bound(s,id) || !seq || s->request.sequence!=seq || s->request.trap!=trap) return false;
    s->request=(struct signal_request_slot){0};return true;
}
static struct signal_wait_slot *current_wait(struct signal_thread_state *s,struct signal_state_identity id,uint64_t ticket) {
    if(!valid_bound(s,id) || s->stack.depth>=4 || !ticket || s->local.flags) return NULL;
    struct signal_wait_slot *w=&s->waits[s->stack.depth];return w->ticket==ticket?w:NULL;
}
bool signal_wait_begin(struct signal_thread_state *s,struct signal_state_identity id,uint64_t anchor,uint64_t oldmask,
    uint64_t temp,uint64_t request_seq,bool info_only,uint64_t *out) {
    if(!out || !valid_bound(s,id) || s->stack.depth>=4 || !user_address(anchor) || s->local.flags ||
       !request_seq || request_seq>s->request_sequence || ((oldmask|temp)&((UINT64_C(1)<<9)|(UINT64_C(1)<<19))) ||
       s->waits[s->stack.depth].ticket || s->io_sequence>=INT64_MAX) return false;
    uint64_t ticket=s->io_sequence+1;
    s->waits[s->stack.depth]=(struct signal_wait_slot){.ticket=ticket,.anchor=anchor,.previous_mask=oldmask,
        .temporary_mask=temp,.request_sequence=request_seq,.phase_flags=SIGNAL_WAIT_PARKED|(info_only?SIGNAL_WAIT_INFO_ONLY:0)};
    s->io_sequence=ticket;*out=ticket;return true;
}
static bool wait_phase(struct signal_thread_state *s,struct signal_state_identity id,uint64_t ticket,uint64_t from,uint64_t to) {
    struct signal_wait_slot *w=current_wait(s,id,ticket);
    if(!w || (w->phase_flags&255)!=from || (w->phase_flags&SIGNAL_WAIT_INFO_COMPLETED)) return false;
    w->phase_flags=(w->phase_flags&~UINT64_C(255))|to;return true;
}
bool signal_wait_wake(struct signal_thread_state *s,struct signal_state_identity id,uint64_t t) {return wait_phase(s,id,t,SIGNAL_WAIT_PARKED,SIGNAL_WAIT_RETURNING);}
bool signal_wait_no_delivery(struct signal_thread_state *s,struct signal_state_identity id,uint64_t t) {return wait_phase(s,id,t,SIGNAL_WAIT_RETURNING,SIGNAL_WAIT_LEGACY_READY);}
bool signal_wait_rearm(struct signal_thread_state *s,struct signal_state_identity id,uint64_t t) {return wait_phase(s,id,t,SIGNAL_WAIT_LEGACY_READY,SIGNAL_WAIT_PARKED);}
bool signal_wait_finish(struct signal_thread_state *s,struct signal_state_identity id,uint64_t ticket,bool legacy,
    uint64_t *mask,bool *restore) {
    struct signal_wait_slot *w=current_wait(s,id,ticket);if(!w || !mask || !restore) return false;
    uint64_t phase=w->phase_flags&255;
    if(phase!=SIGNAL_WAIT_INFO_DONE && !(phase==SIGNAL_WAIT_LEGACY_READY && (legacy || (w->phase_flags&SIGNAL_WAIT_INFO_COMPLETED)))) return false;
    uint64_t old=w->previous_mask;bool change=phase!=SIGNAL_WAIT_INFO_DONE;
    *w=(struct signal_wait_slot){0};*mask=old;*restore=change;return true;
}
bool signal_wait_push_record(struct signal_thread_state *s,struct signal_state_identity id,uint64_t ticket,
    const struct signal_context_record *record,struct signal_context_range range) {
    struct signal_wait_slot *w=current_wait(s,id,ticket);
    if(!w || !record || record->wait_ticket || record->wait_mode || s->stack.depth==4 || s->request.sequence ||
       !user_address(range.start) || range.end<=range.start || !user_address(range.end-1) ||
       range.end-range.start>SIGNAL_CONTEXT_MAX_FRAME || record->frame_identity!=range.start) return false;
    uint64_t phase=w->phase_flags&255;
    if(phase!=SIGNAL_WAIT_RETURNING && phase!=SIGNAL_WAIT_LEGACY_READY) return false;
    for(unsigned i=0;i<s->stack.depth;i++) if(range.start<s->ranges[i].end && s->ranges[i].start<range.end) return false;
    struct signal_thread_state next=*s;unsigned depth=next.stack.depth;
    struct signal_context_record rec=*record;rec.wait_ticket=ticket;
    rec.wait_mode=phase==SIGNAL_WAIT_RETURNING?SIGNAL_WAIT_MASK_OVERRIDE:SIGNAL_WAIT_COMPLETION_ONLY;
    if(rec.wait_mode==SIGNAL_WAIT_MASK_OVERRIDE)rec.mask=w->previous_mask;
    if(!signal_context_push(&next.stack,&rec,id.image_generation,id.scheduler_identity))return false;
    next.ranges[depth]=range;next.waits[depth].phase_flags=(w->phase_flags&~UINT64_C(255))|SIGNAL_WAIT_HANDLER;
    next.waits[depth].record_token=rec.token;
    for(unsigned i=0;i<4;i++) if(next.io[i].flags&SIGNAL_IO_LIVE) next.io[i].flags|=SIGNAL_IO_INTERRUPTED;
    if(!valid_bound(&next,id))return false;*s=next;return true;
}
bool signal_wait_pop_record(struct signal_thread_state *s,struct signal_state_identity id,uint64_t token,uint64_t frame,
    struct signal_context_record *out,struct signal_context_range *range_out) {
    if(!out || !range_out || !valid_bound(s,id) || !s->stack.depth || s->local.flags || s->request.sequence) return false;
    for(unsigned i=0;i<4;i++) if((s->io[i].flags&SIGNAL_IO_LIVE) && s->io[i].depth==s->stack.depth) return false;
    unsigned depth=s->stack.depth-1;const struct signal_context_record *old=&s->stack.records[depth];
    if(!old->wait_ticket || s->waits[depth].ticket!=old->wait_ticket || s->waits[depth].record_token!=token) return false;
    struct signal_thread_state next=*s;struct signal_context_record rec;struct signal_context_range range=next.ranges[depth];
    if(!signal_context_pop(&next.stack,&rec,id.image_generation,id.scheduler_identity,token,frame))return false;
    next.ranges[depth]=(struct signal_context_range){0};struct signal_wait_slot *w=&next.waits[depth];
    w->record_token=0;w->phase_flags=(w->phase_flags&~UINT64_C(255))|
        (rec.wait_mode==SIGNAL_WAIT_MASK_OVERRIDE?SIGNAL_WAIT_INFO_DONE:SIGNAL_WAIT_LEGACY_READY);
    if(rec.wait_mode==SIGNAL_WAIT_COMPLETION_ONLY)w->phase_flags|=SIGNAL_WAIT_INFO_COMPLETED;
    if(!valid_bound(&next,id))return false;*s=next;*out=rec;*range_out=range;return true;
}
