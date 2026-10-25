#include <srvros/audio.h>
#include <srvros/gui.h>
#include <srvros/backlight.h>
#include <srvros/keyboard.h>
#include <srvros/mouse.h>
#include <srvros/heap.h>
#include <srvros/process.h>
#include <srvros/desktop_protocol.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(struct desk_packet)<=GUI_TEXT_MAX,"desktop packet fits transport");

#define GUI_QUEUE_DEPTH DESK_GUI_QUEUE_DEPTH
#define GUI_SURFACE_MAX_PIXELS (DESK_MAX_WIDTH * DESK_MAX_HEIGHT)

struct gui_queue {
    struct gui_queue *next;
    uint64_t pid;
    uint64_t head;
    uint64_t count,cancel_epoch;
    struct gui_message messages[GUI_QUEUE_DEPTH];
};

static struct gui_queue *queues;
static uint64_t queue_bytes;
static uint64_t server_pid;
static uint64_t next_surface_id = 1;

struct gui_surface {
    struct gui_surface *next;
    bool used;
    uint64_t id;
    uint64_t owner_pid;
    uint64_t width;
    uint64_t height;
    uint32_t *pixels;
    bool stream, sealed;
    uint64_t serial, generation, window;
};

static struct gui_surface *surfaces;
/* Charges include prepared and detached storage until it is actually freed. */
static uint64_t surface_pixel_bytes,surface_record_bytes,gui_retirement_epoch;
#ifdef SRVROS_GUI_HOST_TEST
static unsigned gui_test_irq_depth;
#endif

static uint64_t irq_save(void) {
#ifdef SRVROS_GUI_HOST_TEST
    return gui_test_irq_depth++;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}

static void irq_restore(uint64_t flags) {
#ifdef SRVROS_GUI_HOST_TEST
    gui_test_irq_depth=(unsigned)flags;
#else
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
#endif
}

/* Lookup/publication is serialized; candidate allocation is deliberately not. */
static struct gui_queue *queue_for_pid(uint64_t pid) {
    for(struct gui_queue *q=queues;q;q=q->next)if(q->pid==pid)return q;
    return NULL;
}
static void queue_storage_free(struct gui_queue *queue) {
    if(!queue)return;
    kfree(queue);
    uint64_t flags=irq_save();queue_bytes-=sizeof(struct gui_queue);irq_restore(flags);
}
static struct gui_queue *queue_prepare(uint64_t pid) {
    uint64_t flags=irq_save();
    if(queue_bytes>DESK_GUI_QUEUE_BYTES-sizeof(struct gui_queue)) {
        irq_restore(flags);return NULL;
    }
    queue_bytes+=sizeof(struct gui_queue);irq_restore(flags);
    struct gui_queue *queue=kmalloc(sizeof(*queue));
    if(queue) {
        /* Unused message cells are never read; zero only the published header. */
        queue->next=NULL;queue->pid=pid;queue->head=0;queue->count=0;queue->cancel_epoch=0;
    } else {
        flags=irq_save();queue_bytes-=sizeof(struct gui_queue);irq_restore(flags);
    }
    return queue;
}
static int64_t push_message(struct gui_queue *queue,const struct gui_message *message) {
    if(!queue || queue->count>=GUI_QUEUE_DEPTH)return -1;
    uint64_t index=(queue->head+queue->count)%GUI_QUEUE_DEPTH;
    queue->messages[index]=*message;queue->count++;
    process_file_poll_wake();return 0;
}

static bool surface_access_allowed(const struct gui_surface *surface, uint64_t pid) {
    return surface != NULL && surface->used &&
        (surface->owner_pid == pid || (server_pid != 0 && server_pid == pid));
}

static struct gui_surface *surface_for_id(uint64_t surface_id) {
    for(struct gui_surface *s=surfaces;s;s=s->next)
        if(s->id==surface_id)return s;
    return NULL;
}
/* Detach while serialized; release storage outside the GUI metadata section.
 * Uncharge last so another creator cannot spend bytes still being retired. */
static void surface_storage_free(struct gui_surface *surface) {
    uint64_t bytes=surface->width*surface->height*sizeof(uint32_t);
    kfree(surface->pixels);kfree(surface);
    uint64_t flags=irq_save();
    surface_pixel_bytes-=bytes;surface_record_bytes-=sizeof(struct gui_surface);
    irq_restore(flags);
}

static int64_t validate_surface_rect(const struct gui_surface *surface,
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    uint64_t stride) {
    if (surface == NULL || width == 0 || height == 0 || stride < width ||
        x >= surface->width || y >= surface->height ||
        width > surface->width - x || height > surface->height - y ||
        height - 1 > UINT64_MAX / stride ||
        (height - 1) * stride > UINT64_MAX - width ||
        ((height - 1) * stride + width) > UINT64_MAX / sizeof(uint32_t)) {
        return -1;
    }
    return 0;
}

int64_t gui_audio_request(uint64_t operation,uint64_t value) {
    struct process *p=process_current();uint64_t flags=irq_save();int64_t result=-1;
    if(p && server_pid && process_pid(p)==server_pid && operation<=UINT32_MAX && value<=UINT32_MAX)
        result=audio_request((unsigned)operation,(unsigned)value);
    irq_restore(flags);return result;
}
int64_t gui_brightness(uint64_t percent) {
    struct process *p=process_current();uint64_t flags=irq_save();int64_t result=-1;
    if(p && server_pid && process_pid(p)==server_pid && (percent==0 || (percent>=5 && percent<=100)))
        result=backlight_percent((unsigned)percent);
    irq_restore(flags);return result;
}
int64_t gui_pointer_config(uint64_t speed,uint64_t options) {
    struct process *p=process_current();uint64_t flags=irq_save();int64_t result=-1;
    if(p && server_pid && process_pid(p)==server_pid && speed>=1 && speed<=5 && !(options&~(uint64_t)SRV_POINTER_FLAGS)) {
        mouse_trackpad_settings((unsigned)speed,(unsigned)options);result=0;
    }
    irq_restore(flags);return result;
}
int64_t gui_keyboard_mode(uint64_t enabled) {
    struct process *p=process_current();
    uint64_t flags=irq_save();
    int64_t result=-1;
    if (p && server_pid && process_pid(p)==server_pid && enabled<=1) {
        keyboard_set_event_mode(enabled!=0); result=0;
    }
    irq_restore(flags); return result;
}
int64_t gui_keyboard_read(struct srv_key_event *event) {
    struct process *p=process_current();
    uint64_t flags=irq_save();
    int64_t result=-1;
    if (p && server_pid && process_pid(p)==server_pid)
        result=keyboard_read_event(event)?1:0;
    irq_restore(flags); return result;
}
int64_t gui_register_server(void) {
    struct process *process=process_current();
    if(!process)return -1;
    uint64_t pid=process_pid(process),flags=irq_save(),epoch=gui_retirement_epoch;
    uint64_t original_server=server_pid;
    struct gui_queue *candidate=NULL;
    int64_t result=-1;
    if(server_pid && server_pid!=pid)goto done;
    if(!queue_for_pid(pid)) {
        irq_restore(flags);candidate=queue_prepare(pid);flags=irq_save();
        if(!candidate || epoch!=gui_retirement_epoch || server_pid!=original_server)goto done;
    }
    if(!queue_for_pid(pid)) {
        candidate->next=queues;queues=candidate;candidate=NULL;
    }
    server_pid=pid;result=0;
done:
    irq_restore(flags);queue_storage_free(candidate);return result;
}

int64_t gui_send(const struct gui_message *message) {
    struct process *process = process_current();
    uint64_t flags;
    int64_t result;
    if (process == NULL || message == NULL) {
        return -1;
    }

    struct gui_queue *candidate=NULL;
    bool prepared=false;
    uint64_t epoch=0,original_server=0;
    uint64_t source_pid=process_pid(process);
retry:
    flags = irq_save();
    if(prepared && (epoch!=gui_retirement_epoch || original_server!=server_pid))goto rejected;
    uint64_t target_pid = source_pid == server_pid ? message->target_pid : server_pid;
    if(target_pid==0)goto rejected;

    struct gui_surface *transition = NULL;
    struct desk_packet packet = {0};
    bool publishing = false;
    if (message->type == DESK_TRANSPORT_MESSAGE) {
        for (size_t i=0;i<sizeof(packet);i++) ((uint8_t *)&packet)[i]=(uint8_t)message->text[i];
        if (packet.version != DESK_PROTOCOL_VERSION || packet.size != sizeof(packet) ||
            packet.reserved || packet.reserved2 || !packet.window || packet.type < DESK_OPEN || packet.type > DESK_CLOSE_ACK)
            goto rejected;
        bool from_server = source_pid == server_pid;
        if ((!from_server && packet.type != DESK_OPEN && packet.type != DESK_COMMIT && packet.type != DESK_CLOSE &&
             packet.type != DESK_RESIZE_REFUSED && packet.type != DESK_CANCEL_CLOSE && packet.type != DESK_TITLE && packet.type != DESK_LAUNCH && packet.type != DESK_PREFERENCES_SET && packet.type != DESK_CLIPBOARD_SET && packet.type != DESK_CLIPBOARD_GET && packet.type != DESK_MENU_SET && packet.type != DESK_NOTIFY && packet.type != DESK_BRIGHTNESS && packet.type != DESK_AUDIO) ||
            (from_server && (packet.type == DESK_RESIZE_REFUSED || packet.type == DESK_OPEN || packet.type == DESK_COMMIT ||
                            packet.type == DESK_CANCEL_CLOSE || packet.type == DESK_TITLE || packet.type == DESK_LAUNCH || packet.type == DESK_PREFERENCES_SET || packet.type == DESK_CLIPBOARD_SET || packet.type == DESK_CLIPBOARD_GET || packet.type == DESK_MENU_SET || packet.type == DESK_NOTIFY))) goto rejected;
        if (packet.type == DESK_COMMIT || packet.type == DESK_RELEASE) {
            transition = surface_for_id(packet.surface);
            if (!transition || !transition->pixels || !transition->stream) goto rejected;
            publishing = packet.type == DESK_COMMIT;
            if (publishing) {
                if (transition->owner_pid != source_pid || transition->sealed ||
                    (transition->window && transition->window != packet.window) ||
                    !packet.serial || packet.serial <= transition->serial || !packet.generation ||
                    packet.width != transition->width || packet.height != transition->height) goto rejected;
            } else if (transition->owner_pid != target_pid || !transition->sealed ||
                packet.serial != transition->serial || packet.generation != transition->generation ||
                packet.window != transition->window) goto rejected;
        }
    }

    struct gui_queue *queue=queue_for_pid(target_pid);
    if(!queue && !prepared) {
        epoch=gui_retirement_epoch;original_server=server_pid;
        irq_restore(flags);candidate=queue_prepare(target_pid);
        if(!candidate)return -1;
        prepared=true;goto retry;
    }
    /* A competing publisher may have installed the destination while this
     * candidate was prepared. Use its FIFO, then free our unused reservation. */
    if(!queue) {
        queue=candidate;candidate=NULL;queue->next=queues;queues=queue;
    }
    struct gui_message copy = *message;
    copy.source_pid = source_pid;
    copy.target_pid = target_pid;
    result = push_message(queue, &copy);
    /* Queue insertion and ownership transition are one transaction. A full
     * queue leaves the sender's buffer/release retryable, never stranded. */
    if (result == 0 && transition) {
        transition->sealed = publishing;
        if (publishing) { transition->serial=packet.serial; transition->generation=packet.generation; transition->window=packet.window; }
    }
    irq_restore(flags);queue_storage_free(candidate);
    return result;
rejected:
    irq_restore(flags);queue_storage_free(candidate);
    return -1;
}

int64_t gui_recv(struct gui_message *message) {
    struct process *process = process_current();
    uint64_t flags;
    int64_t result = 1;
    if (process == NULL || message == NULL) {
        return -1;
    }

    flags = irq_save();
    struct gui_queue *queue = queue_for_pid(process_pid(process));
    if (queue == NULL || queue->count == 0) {
        irq_restore(flags);
        return 0;
    }

    *message = queue->messages[queue->head];
    queue->head = (queue->head + 1) % GUI_QUEUE_DEPTH;
    queue->count--;
    irq_restore(flags);
    return result;
}

int64_t gui_server_ready(void) {
    struct process *process=process_current();
    if(!process) return -1;
    uint64_t flags=irq_save(),pid=process_pid(process);
    int64_t result=-1;
    if(server_pid && pid==server_pid) {
        struct gui_queue *queue=queue_for_pid(pid);
        result=(queue && queue->count) || keyboard_events_pending() || mouse_pending();
    }
    irq_restore(flags);return result;
}

int64_t gui_client_ready(uint64_t expected_server) {
    struct process *process=process_current();
    if(!process || !expected_server) return -1;
    uint64_t flags=irq_save(),pid=process_pid(process);
    int64_t result=-1;
    if(server_pid==expected_server && pid!=server_pid) {
        struct gui_queue *queue=queue_for_pid(pid);
        result=queue && queue->count?1:0;
    }
    irq_restore(flags);return result;
}

uint32_t gui_client_wait_state(uint64_t server,uint64_t expected_epoch,uint64_t *observed_epoch) {
    struct process *process=process_current();
    if(!process || !server)return SRV_GUI_DISCONNECTED;
    uint64_t flags=irq_save(),pid=process_pid(process);
    struct gui_queue *queue=queue_for_pid(pid);
    uint32_t result=SRV_GUI_DISCONNECTED;
    if(server_pid==server && pid!=server_pid && queue) {
        *observed_epoch=queue->cancel_epoch;
        result=queue->count?SRV_GUI_READY:0;
        if(queue->cancel_epoch!=expected_epoch || queue->cancel_epoch==INT64_MAX)result|=SRV_GUI_CANCELED;
    }
    irq_restore(flags);return result;
}
int64_t gui_cancel_wait(void) {
    struct process *process=process_current();if(!process)return -1;
    uint64_t flags=irq_save(),pid=process_pid(process);
    struct gui_queue *queue=queue_for_pid(pid);int64_t result=-1;
    if(server_pid && pid!=server_pid && queue) {
        if(queue->cancel_epoch<INT64_MAX)queue->cancel_epoch++;
        result=queue->cancel_epoch;process_file_poll_wake();
    }
    irq_restore(flags);return result;
}

int64_t gui_surface_create(uint64_t owner_pid, uint64_t width, uint64_t height, uint64_t options) {
    if ((options & ~(uint64_t)DESK_SURFACE_STREAM) || owner_pid == 0 || width == 0 || height == 0 ||
        width > GUI_SURFACE_MAX_PIXELS || height > GUI_SURFACE_MAX_PIXELS ||
        width > UINT64_MAX / height || width * height > GUI_SURFACE_MAX_PIXELS ||
        width * height > SIZE_MAX / sizeof(uint32_t))return -1;
    uint64_t bytes=width*height*sizeof(uint32_t),flags=irq_save();
    if(surface_pixel_bytes>DESK_GUI_PIXEL_BYTES || bytes>DESK_GUI_PIXEL_BYTES-surface_pixel_bytes ||
       surface_record_bytes>DESK_GUI_SURFACE_RECORD_BYTES-sizeof(struct gui_surface) ||
       next_surface_id>INT64_MAX) { irq_restore(flags);return -1; }
    surface_pixel_bytes+=bytes;surface_record_bytes+=sizeof(struct gui_surface);
    uint64_t epoch=gui_retirement_epoch;
    irq_restore(flags);

    struct gui_surface *slot=kmalloc(sizeof(*slot));
    uint32_t *pixels=slot?kmalloc((size_t)bytes):NULL;
    if(pixels)for(uint64_t i=0;i<width*height;i++)pixels[i]=0xff000000u;
    flags=irq_save();
    /* Cleanup during preparation invalidates publication. A conservative
     * refusal on unrelated retirement is retryable by creating a new window;
     * it must never resurrect storage for a retired owner. */
    if(!pixels || epoch!=gui_retirement_epoch || next_surface_id>INT64_MAX) {
        irq_restore(flags);kfree(pixels);kfree(slot);
        flags=irq_save();surface_pixel_bytes-=bytes;surface_record_bytes-=sizeof(struct gui_surface);
        irq_restore(flags);return -1;
    }
    uint64_t id=next_surface_id++;
    *slot=(struct gui_surface){.next=surfaces,.used=true,.id=id,.owner_pid=owner_pid,
        .width=width,.height=height,.pixels=pixels,.stream=(options&DESK_SURFACE_STREAM)!=0};
    surfaces=slot;irq_restore(flags);return (int64_t)id;
}

int64_t gui_surface_destroy(uint64_t pid, uint64_t surface_id) {
    uint64_t flags=irq_save();
    struct gui_surface **link=&surfaces;
    while(*link && (*link)->id!=surface_id)link=&(*link)->next;
    struct gui_surface *surface=*link;
    if(!surface_access_allowed(surface,pid) || surface->sealed) { irq_restore(flags);return -1; }
    *link=surface->next;surface->used=false;
    irq_restore(flags);surface_storage_free(surface);return 0;
}

int64_t gui_surface_blit(uint64_t pid,
    uint64_t surface_id,
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    const uint32_t *pixels,
    uint64_t stride) {
    if (pixels == NULL) {
        return -1;
    }
    uint64_t flags_saved = irq_save();
    struct gui_surface *surface = surface_for_id(surface_id);
    if (!surface_access_allowed(surface, pid) || surface->sealed ||
        validate_surface_rect(surface, x, y, width, height, stride) != 0 ||
        surface->pixels == NULL) {
        irq_restore(flags_saved);
        return -1;
    }
    for (uint64_t row = 0; row < height; row++) {
        uint32_t *to = surface->pixels + (y + row) * surface->width + x;
        const uint32_t *from = pixels + row * stride;
        for (uint64_t col = 0; col < width; col++) {
            to[col] = from[col];
        }
    }
    irq_restore(flags_saved);
    return 0;
}

int64_t gui_surface_copy(uint64_t pid,
    uint64_t surface_id,
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    uint32_t *pixels,
    uint64_t stride) {
    if (pixels == NULL) {
        return -1;
    }
    uint64_t flags_saved = irq_save();
    struct gui_surface *surface = surface_for_id(surface_id);
    if (!surface_access_allowed(surface, pid) ||
        validate_surface_rect(surface, x, y, width, height, stride) != 0 ||
        surface->pixels == NULL) {
        irq_restore(flags_saved);
        return -1;
    }
    for (uint64_t row = 0; row < height; row++) {
        const uint32_t *from = surface->pixels + (y + row) * surface->width + x;
        uint32_t *to = pixels + row * stride;
        for (uint64_t col = 0; col < width; col++) {
            to[col] = from[col];
        }
    }
    irq_restore(flags_saved);
    return 0;
}

void gui_process_cleanup(uint64_t pid) {
    struct gui_surface *retired=NULL;
    uint64_t flags_saved=irq_save();
    gui_retirement_epoch++;
    struct gui_queue **queue_link=&queues,*retired_queue=NULL;
    while(*queue_link) {
        if((*queue_link)->pid==pid) {
            retired_queue=*queue_link;*queue_link=retired_queue->next;break;
        }
        queue_link=&(*queue_link)->next;
    }
    if (server_pid == pid) {
        keyboard_set_event_mode(false);
        mouse_trackpad_settings(3,0);
        server_pid = 0;
        /* The server queue is gone; outstanding owners may reclaim buffers. */
        for(struct gui_surface *s=surfaces;s;s=s->next)s->sealed=false;
    }
    struct gui_surface **link=&surfaces;
    while(*link) {
        struct gui_surface *s=*link;
        if(s->owner_pid!=pid) { link=&s->next;continue; }
        *link=s->next;s->used=false;s->next=retired;retired=s;
    }
    process_file_poll_wake();
    irq_restore(flags_saved);
    queue_storage_free(retired_queue);
    while(retired) {
        struct gui_surface *next=retired->next;
        surface_storage_free(retired);retired=next;
    }
}
