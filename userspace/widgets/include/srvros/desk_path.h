#ifndef SRVROS_DESK_PATH_H
#define SRVROS_DESK_PATH_H
#include <stddef.h>
#define DESK_PATH_INPUT_LIMIT 4096u
/* Lexical UTF-8 path operations, without I/O or symbolic-link resolution.
 * Normalize absolute paths, collapse repeated slashes and dot components,
 * remove trailing slashes except root, and clamp .. at root. All operations
 * leave output unchanged on failure and allow input/output overlap. */
int desk_path_normalize(char *output,size_t capacity,const char *input);
int desk_path_join(char *output,size_t capacity,const char *directory,const char *name);
int desk_path_parent(char *output,size_t capacity,const char *input);
int desk_path_name_valid(const char *name);
#endif
