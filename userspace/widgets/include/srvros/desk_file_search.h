#ifndef SRVROS_DESK_FILE_SEARCH_H
#define SRVROS_DESK_FILE_SEARCH_H
#include <srvros/desktop_search.h>
#include <stddef.h>
/* Literal UTF-8 file-name matching, with ASCII-only case folding. These helpers
 * do no I/O. Paths are canonical absolute paths without dot components. */
int desk_file_search_path_valid(const char *path);
int desk_file_search_query_valid(const char *query);
int desk_file_search_under(const char *path,const char *root);
int desk_file_search_matches(const char *path,const char *query);
/* Validate the complete worker snapshot before changing caller storage. Entries
 * must be unique paths under root, match query/kind, and carry valid metadata.
 * kind: 0 all, 1 files, 2 folders. */
int desk_file_search_decode(struct desk_search_header *header,struct desk_search_entry *entries,
    const void *data,size_t length,const char *root,const char *query,unsigned kind);
#endif
