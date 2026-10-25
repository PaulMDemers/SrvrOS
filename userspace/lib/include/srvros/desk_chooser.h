#ifndef SRVROS_DESK_CHOOSER_H
#define SRVROS_DESK_CHOOSER_H
#include <srvros/desk_file_picker.h>
#include <srvros/desk_listing.h>
#include <srvros/desk_file_job.h>

/* Optional application-runtime library, above the pure picker widget. All
 * storage is borrowed and remains at a stable address until pending==0.
 * Initialize once per owner; one dispatcher calls all functions. The caller
 * owns worker polling/reaping, focus, redraw, clipboard and document I/O.
 * Do not reuse/mutate the job while pending; finish takes its poll result.
 * Cancel hides selection but never kills a worker or cancels accepted I/O. */
struct desk_chooser {
    struct desk_file_picker *picker;
    struct desk_file_job *job;
    struct desk_file_entry *entries;
    struct desk_list_item *items;
    char (*details)[32];
    uint64_t next_id,pending_id;
    uint32_t wire_id;
    int active,pending,count;
};
struct desk_chooser_selection {
    enum desk_picker_mode mode;
    char path[DESK_PICKER_PATH];
};
enum desk_chooser_action { DESK_CHOOSER_REFUSED=-1,DESK_CHOOSER_NONE=0,
    DESK_CHOOSER_ACCEPTED=1,DESK_CHOOSER_CANCELED=2 };
enum desk_chooser_completion { DESK_CHOOSER_STALE=-2,DESK_CHOOSER_ERROR=-1,
    DESK_CHOOSER_DISCARDED=0,DESK_CHOOSER_READY=1 };
/* Listing arrays each contain DESK_FILE_ENTRIES elements, as for decode. */
int desk_chooser_init(struct desk_chooser *chooser,struct desk_file_picker *picker,
    struct desk_file_job *job,struct desk_file_entry *entries,
    struct desk_list_item *items,char details[][32]);
/* Open validates/initializes the widget; caller configures its geometry then
 * calls browse. Refuse active/pending/foreign busy work without changing it. */
int desk_chooser_open(struct desk_chooser *chooser,enum desk_picker_mode mode,
    const char *directory,const char *name);
int desk_chooser_browse(struct desk_chooser *chooser);
void desk_chooser_cancel(struct desk_chooser *chooser);
/* Process only the operation; clipboard/redraw flags remain caller policy.
 * ACCEPT copies a normalized path before hiding. No file read/write is started. */
int desk_chooser_action(struct desk_chooser *chooser,struct desk_picker_result result,
    struct desk_chooser_selection *selection);
/* Snapshot pending_id when polling this job; pass it with its terminal result.
 * IDs never wrap, including across legacy worker replacement. Stale/duplicate,
 * still-busy or foreign completion cannot change the listing or release ownership.
 * A canceled terminal result is drained/discarded, then reopening is possible. */
int desk_chooser_finish(struct desk_chooser *chooser,uint64_t id,int result);
#endif
