#ifndef SRVROS_DESK_FILE_PICKER_H
#define SRVROS_DESK_FILE_PICKER_H
#include <srvros/desk_ui.h>
#define DESK_PICKER_PATH 160
#define DESK_PICKER_NAME 96
enum desk_picker_mode { DESK_PICKER_OPEN=1,DESK_PICKER_SAVE };
enum desk_picker_operation { DESK_PICKER_NONE,DESK_PICKER_BROWSE,DESK_PICKER_ACCEPT,DESK_PICKER_CANCEL };
enum desk_picker_control { DESK_PICKER_UP=1,DESK_PICKER_REFRESH,DESK_PICKER_OK,DESK_PICKER_DISMISS,
    DESK_PICKER_LOCATION,DESK_PICKER_FILENAME,DESK_PICKER_LIST };
struct desk_picker_result { enum desk_picker_operation operation; unsigned flags,field; };
/* Presentation only: no filesystem calls. Listings and their strings are
 * borrowed until replaced. BROWSE/ACCEPT return an absolute path in target.
 * The app calls loading before asynchronous I/O, then listing or error.
 * Cancel remains available during I/O; the caller drains/discards its worker
 * response. Filename validation is lexical; existence/overwrite checks belong
 * to the application. Clipboard flags identify the relevant editor field. */
struct desk_file_picker {
    struct desk_ui ui;
    struct desk_button buttons[4];
    struct desk_edit location,name;
    struct desk_text_buffer location_text,name_text;
    char location_data[2][DESK_PICKER_PATH],name_data[2][DESK_PICKER_NAME];
    struct desk_list list;
    struct desk_rect frame;
    char directory[DESK_PICKER_PATH],target[DESK_PICKER_PATH],status[160];
    enum desk_picker_mode mode;
    int busy;
};
void desk_picker_init(struct desk_file_picker *picker);
int desk_picker_open(struct desk_file_picker *picker,enum desk_picker_mode mode,const char *directory,const char *name);
void desk_picker_configure(struct desk_file_picker *picker,struct desk_rect bounds);
void desk_picker_loading(struct desk_file_picker *picker);
int desk_picker_listing(struct desk_file_picker *picker,const char *directory,const struct desk_list_item *items,int count);
void desk_picker_message(struct desk_file_picker *picker,const char *message);
void desk_picker_error(struct desk_file_picker *picker,const char *message);
unsigned desk_picker_focus(struct desk_file_picker *picker,int active);
struct desk_picker_result desk_picker_key(struct desk_file_picker *picker,const struct srv_key_event *key);
struct desk_picker_result desk_picker_pointer(struct desk_file_picker *picker,struct desk_widget_event event,uint64_t time_us);
struct desk_picker_result desk_picker_scroll(struct desk_file_picker *picker,int x,int y,int delta);
void desk_picker_draw(struct desk_canvas *canvas,const struct desk_file_picker *picker,const struct desk_theme *theme);
#endif
