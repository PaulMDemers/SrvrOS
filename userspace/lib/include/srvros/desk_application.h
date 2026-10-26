#ifndef SRVROS_DESK_APPLICATION_H
#define SRVROS_DESK_APPLICATION_H
#include <stdint.h>
#include <srvros/desk_app.h>
/* Application-wide intent, distinct from each document's unsaved-work policy.
 * Resolve one close at a time. Cancellation/save failure stops the remaining
 * quit; documents already retired remain closed. Last-window close exits. */
struct desk_application_lifecycle { int quitting;uint64_t closing_window; };
void desk_application_quit(struct desk_application_lifecycle *application);
int desk_application_close_next(struct desk_application_lifecycle *application,uint64_t window);
void desk_application_closed(struct desk_application_lifecycle *application,uint64_t window);
void desk_application_cancel_quit(struct desk_application_lifecycle *application,uint64_t window);
/* One receiver per application; advertise only one live document window.
 * Retain path until accepted/refused; tick retries the acknowledgement. */
struct desk_open_document {
    uint64_t request,last_request;
    unsigned length;int ready,reply_pending;
    char path[160];struct desk_packet reply;
};
int desk_application_service(struct desk_app *window,unsigned capabilities);
int desk_open_document_receive(struct desk_open_document *request,const struct desk_packet *packet);
void desk_open_document_finish(struct desk_open_document *request,int accepted);
void desk_open_document_tick(struct desk_open_document *request,uint64_t server);
#endif
