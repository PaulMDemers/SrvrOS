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

#define GUI_MAX_QUEUES 16
#define GUI_QUEUE_DEPTH 64
#define GUI_MAX_SURFACES 32
#define GUI_SURFACE_MAX_PIXELS (DESK_MAX_WIDTH * DESK_MAX_HEIGHT)

struct gui_queue {
    bool used;
    uint64_t pid;
    uint64_t head;
    uint64_t count;
    struct gui_message messages[GUI_QUEUE_DEPTH];
};

static struct gui_queue queues[GUI_MAX_QUEUES];
static uint64_t server_pid;
static uint64_t next_surface_id = 1;

struct gui_surface {
    bool used;
    uint64_t id;
    uint64_t owner_pid;
    uint64_t width;
    uint64_t height;
    uint32_t *pixels;
    bool stream, sealed;
    uint64_t serial, generation;
};

static struct gui_surface surfaces[GUI_MAX_SURFACES];

static uint64_t irq_save(void) {
#ifdef SRVROS_GUI_HOST_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}

static void irq_restore(uint64_t flags) {
#ifdef SRVROS_GUI_HOST_TEST
    (void)flags;
#else
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
#endif
}

static struct gui_queue *queue_for_pid(uint64_t pid, bool create) {
    struct gui_queue *free_queue = NULL;
    for (uint64_t i = 0; i < GUI_MAX_QUEUES; i++) {
        if (queues[i].used && queues[i].pid == pid) {
            return &queues[i];
        }
        if (!queues[i].used && free_queue == NULL) {
            free_queue = &queues[i];
        }
    }

    if (!create || free_queue == NULL) {
        return NULL;
    }

    free_queue->used = true;
    free_queue->pid = pid;
    free_queue->head = 0;
    free_queue->count = 0;
    return free_queue;
}

static int64_t push_message(uint64_t target_pid, const struct gui_message *message) {
    struct gui_queue *queue = queue_for_pid(target_pid, true);
    if (queue == NULL || queue->count >= GUI_QUEUE_DEPTH) {
        return -1;
    }

    uint64_t index = (queue->head + queue->count) % GUI_QUEUE_DEPTH;
    queue->messages[index] = *message;
    queue->count++;
    return 0;
}

static bool surface_access_allowed(const struct gui_surface *surface, uint64_t pid) {
    return surface != NULL && surface->used &&
        (surface->owner_pid == pid || (server_pid != 0 && server_pid == pid));
}

static struct gui_surface *surface_for_id(uint64_t surface_id) {
    for (uint64_t i = 0; i < GUI_MAX_SURFACES; i++) {
        if (surfaces[i].used && surfaces[i].id == surface_id) {
            return &surfaces[i];
        }
    }
    return NULL;
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
    struct process *process = process_current();
    uint64_t flags;
    int64_t result;
    if (process == NULL) {
        return -1;
    }

    flags = irq_save();
    uint64_t pid = process_pid(process);
    /* Do not let a second compositor steal access to every client's surfaces.
     * Publish ownership only after its receive queue has been allocated. */
    result = -1;
    if ((server_pid == 0 || server_pid == pid) && queue_for_pid(pid, true) != NULL) {
        server_pid = pid;
        result = 0;
    }
    irq_restore(flags);
    return result;
}

int64_t gui_send(const struct gui_message *message) {
    struct process *process = process_current();
    uint64_t flags;
    int64_t result;
    if (process == NULL || message == NULL) {
        return -1;
    }

    flags = irq_save();
    uint64_t source_pid = process_pid(process);
    uint64_t target_pid = source_pid == server_pid ? message->target_pid : server_pid;
    if (target_pid == 0) {
        irq_restore(flags);
        return -1;
    }

    struct gui_surface *transition = NULL;
    struct desk_packet packet = {0};
    bool publishing = false;
    if (message->type == DESK_TRANSPORT_MESSAGE) {
        for (size_t i=0;i<sizeof(packet);i++) ((uint8_t *)&packet)[i]=(uint8_t)message->text[i];
        if (packet.version != DESK_PROTOCOL_VERSION || packet.size != sizeof(packet) ||
            packet.reserved || packet.reserved2 || packet.type < DESK_OPEN || packet.type > DESK_AUDIO)
            goto rejected;
        bool from_server = source_pid == server_pid;
        if ((!from_server && packet.type != DESK_OPEN && packet.type != DESK_COMMIT && packet.type != DESK_CLOSE &&
             packet.type != DESK_CANCEL_CLOSE && packet.type != DESK_TITLE && packet.type != DESK_LAUNCH && packet.type != DESK_PREFERENCES_SET && packet.type != DESK_CLIPBOARD_SET && packet.type != DESK_CLIPBOARD_GET && packet.type != DESK_MENU_SET && packet.type != DESK_NOTIFY && packet.type != DESK_BRIGHTNESS && packet.type != DESK_AUDIO) ||
            (from_server && (packet.type == DESK_OPEN || packet.type == DESK_COMMIT ||
                            packet.type == DESK_CANCEL_CLOSE || packet.type == DESK_TITLE || packet.type == DESK_LAUNCH || packet.type == DESK_PREFERENCES_SET || packet.type == DESK_CLIPBOARD_SET || packet.type == DESK_CLIPBOARD_GET || packet.type == DESK_MENU_SET || packet.type == DESK_NOTIFY))) goto rejected;
        if (packet.type == DESK_COMMIT || packet.type == DESK_RELEASE) {
            transition = surface_for_id(packet.surface);
            if (!transition || !transition->pixels || !transition->stream) goto rejected;
            publishing = packet.type == DESK_COMMIT;
            if (publishing) {
                if (transition->owner_pid != source_pid || transition->sealed ||
                    !packet.serial || packet.serial <= transition->serial || !packet.generation ||
                    packet.width != transition->width || packet.height != transition->height) goto rejected;
            } else if (transition->owner_pid != target_pid || !transition->sealed ||
                packet.serial != transition->serial || packet.generation != transition->generation) goto rejected;
        }
    }

    struct gui_message copy = *message;
    copy.source_pid = source_pid;
    copy.target_pid = target_pid;
    result = push_message(target_pid, &copy);
    /* Queue insertion and ownership transition are one transaction. A full
     * queue leaves the sender's buffer/release retryable, never stranded. */
    if (result == 0 && transition) {
        transition->sealed = publishing;
        if (publishing) { transition->serial=packet.serial; transition->generation=packet.generation; }
    }
    irq_restore(flags);
    return result;
rejected:
    irq_restore(flags);
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
    struct gui_queue *queue = queue_for_pid(process_pid(process), false);
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

int64_t gui_surface_create(uint64_t owner_pid, uint64_t width, uint64_t height, uint64_t flags) {
    if ((flags & ~(uint64_t)DESK_SURFACE_STREAM) || owner_pid == 0 || width == 0 || height == 0 ||
        width > GUI_SURFACE_MAX_PIXELS || height > GUI_SURFACE_MAX_PIXELS ||
        width > UINT64_MAX / height || width * height > GUI_SURFACE_MAX_PIXELS ||
        width * height > SIZE_MAX / sizeof(uint32_t)) {
        return -1;
    }

    uint64_t flags_saved = irq_save();
    struct gui_surface *slot = NULL;
    for (uint64_t i = 0; i < GUI_MAX_SURFACES; i++) {
        if (!surfaces[i].used) {
            slot = &surfaces[i];
            break;
        }
    }
    if (slot == NULL) {
        irq_restore(flags_saved);
        return -1;
    }
    slot->used = true;
    slot->id = next_surface_id++;
    if (next_surface_id == 0) {
        next_surface_id = 1;
    }
    slot->owner_pid = owner_pid;
    slot->width = width;
    slot->height = height;
    slot->pixels = NULL;
    slot->stream = (flags & DESK_SURFACE_STREAM) != 0;
    slot->sealed = false;
    slot->serial = slot->generation = 0;
    uint64_t id = slot->id;
    irq_restore(flags_saved);

    uint32_t *pixels = kmalloc((size_t)(width * height * sizeof(uint32_t)));
    if (pixels == NULL) {
        gui_surface_destroy(owner_pid, id);
        return -1;
    }
    for (uint64_t i = 0; i < width * height; i++) {
        pixels[i] = 0xff000000u;
    }

    flags_saved = irq_save();
    slot = surface_for_id(id);
    if (slot == NULL || !slot->used || slot->owner_pid != owner_pid) {
        irq_restore(flags_saved);
        kfree(pixels);
        return -1;
    }
    slot->pixels = pixels;
    irq_restore(flags_saved);
    return (int64_t)id;
}

int64_t gui_surface_destroy(uint64_t pid, uint64_t surface_id) {
    uint32_t *pixels = NULL;
    uint64_t flags_saved = irq_save();
    struct gui_surface *surface = surface_for_id(surface_id);
    if (!surface_access_allowed(surface, pid) || surface->sealed) {
        irq_restore(flags_saved);
        return -1;
    }
    pixels = surface->pixels;
    surface->used = false;
    surface->id = 0;
    surface->owner_pid = 0;
    surface->width = 0;
    surface->height = 0;
    surface->pixels = NULL;
    irq_restore(flags_saved);
    if (pixels != NULL) {
        kfree(pixels);
    }
    return 0;
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
    uint32_t *free_list[GUI_MAX_SURFACES];
    uint64_t free_count = 0;
    uint64_t flags_saved = irq_save();
    for (uint64_t i = 0; i < GUI_MAX_QUEUES; i++) {
        if (queues[i].used && queues[i].pid == pid) {
            queues[i].used = false;
            queues[i].pid = 0;
            queues[i].head = 0;
            queues[i].count = 0;
        }
    }
    if (server_pid == pid) {
        keyboard_set_event_mode(false);
        mouse_trackpad_settings(3,0);
        server_pid = 0;
        /* The server's receive queue is gone. Owners can reclaim outstanding
         * buffers after detecting its death; no old server can acknowledge. */
        for (uint64_t i=0;i<GUI_MAX_SURFACES;i++) surfaces[i].sealed=false;
    }
    for (uint64_t i = 0; i < GUI_MAX_SURFACES; i++) {
        if (surfaces[i].used && surfaces[i].owner_pid == pid) {
            free_list[free_count++] = surfaces[i].pixels;
            surfaces[i].used = false;
            surfaces[i].id = 0;
            surfaces[i].owner_pid = 0;
            surfaces[i].width = 0;
            surfaces[i].height = 0;
            surfaces[i].pixels = NULL;
        }
    }
    irq_restore(flags_saved);
    for (uint64_t i = 0; i < free_count; i++) {
        if (free_list[i] != NULL) {
            kfree(free_list[i]);
        }
    }
}
