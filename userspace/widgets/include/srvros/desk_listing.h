#ifndef SRVROS_DESK_LISTING_H
#define SRVROS_DESK_LISTING_H
#include <srvros/desktop_file.h>
#include <srvros/desk_list.h>
/* Validate a worker snapshot before changing caller storage, then sort folders
 * first. Result items borrow entries/details until the next successful decode.
 * No I/O or allocation; all destination arrays have DESK_FILE_ENTRIES capacity. */
int desk_listing_decode(struct desk_file_entry *entries,struct desk_list_item *items,
    char details[][32],const void *data,size_t length,unsigned count);
#endif
