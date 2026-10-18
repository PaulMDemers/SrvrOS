#ifndef SRVROS_DESK_PREFERENCES_H
#define SRVROS_DESK_PREFERENCES_H
#include <srvros/desktop_preferences.h>
#include <stddef.h>
struct desk_preferences desk_preferences_default(void);
int desk_preferences_valid(const struct desk_preferences *preferences);
/* Validate selected fields and merge them; never partially apply an invalid request. */
int desk_preferences_merge(struct desk_preferences *destination,const struct desk_preferences *source,unsigned mask);
int desk_preferences_decode(struct desk_preferences *preferences,const void *data,size_t length);
int desk_preferences_encode(const struct desk_preferences *preferences,char *out,size_t capacity);
/* UTC seconds -> display text with an explicit fixed offset, no DST assumption. */
int desk_clock_format(char *out,size_t capacity,long utc_seconds,const struct desk_preferences *preferences);
#endif
