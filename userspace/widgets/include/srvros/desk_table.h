#ifndef SRVROS_DESK_TABLE_H
#define SRVROS_DESK_TABLE_H
#include <srvros/desk_list.h>
#define DESK_TABLE_MAX_COLUMNS 8
#define DESK_TABLE_MAX_ROWS 256
#define DESK_TABLE_SORT (1u << 14)
/* A zero width column shares remaining space; fixed columns retain their width.
 * Narrow tables clip trailing columns. Rows and strings are borrowed. Stable,
 * unique row IDs preserve selection across refreshes and application sorting.
 * The control requests sorting; the application owns ordering and data access. */
struct desk_table_column { const char *label; int width, right, sortable; };
struct desk_table_row { uint64_t id; const char *cells[DESK_TABLE_MAX_COLUMNS]; };
struct desk_table {
    struct desk_rect frame;
    const struct desk_table_column *columns;
    const struct desk_table_row *rows;
    int column_count, sort_column, descending, header_focus, header_column, pressed_column;
    struct desk_list body;
    struct desk_list_item navigation[DESK_TABLE_MAX_ROWS];
};
void desk_table_init(struct desk_table *table);
int desk_table_columns(struct desk_table *table,const struct desk_table_column *columns,int count);
int desk_table_rows(struct desk_table *table,const struct desk_table_row *rows,int count);
void desk_table_configure(struct desk_table *table,struct desk_rect frame,int enabled);
void desk_table_sort_indicator(struct desk_table *table,int column,int descending);
unsigned desk_table_event(struct desk_table *table,struct desk_widget_event event,uint64_t time_us);
/* F6 switches between headings and rows; Left/Right chooses a heading, and
 * Enter/Space requests its sort. Row navigation/typeahead matches desk_list. */
unsigned desk_table_key(struct desk_table *table,const struct srv_key_event *key);
unsigned desk_table_scroll(struct desk_table *table,int64_t delta);
void desk_table_draw(struct desk_canvas *canvas,const struct desk_table *table,const struct desk_theme *theme);
#endif
