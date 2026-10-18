#ifndef DESKTOOLS_APP_H
#define DESKTOOLS_APP_H
#include <srvros/desk_app.h>
#include <srvros/desk_ui.h>
#include <srvros/desk_clipboard.h>
#include "file_job.h"
enum tool_action { TOOL_NEW=1,TOOL_OPEN,TOOL_SAVE,TOOL_PATH,TOOL_BODY,TOOL_UP,TOOL_REFRESH,TOOL_MKDIR,TOOL_LIST,TOOL_NAME };
enum tool_pending { TOOL_PENDING_NONE,TOOL_PENDING_NEW,TOOL_PENDING_OPEN,TOOL_PENDING_CLOSE };
struct tool {
    int role,quit,worker_ready,modal,pending,closing,body_size,title_dirty;
    struct desk_app window;
    struct desk_ui ui,dialog;
    struct desk_button buttons[32],dialog_buttons[3];
    struct desk_edit path,body,name;
    struct desk_text_buffer path_text,body_text,name_text;
    char path_store[2][DESK_FILE_PATH],body_store[2][DESK_FILE_BYTES+1],name_store[2][128];
    struct desk_list list;
    struct desk_list_item items[DESK_FILE_ENTRIES];
    struct desk_file_entry entries[DESK_FILE_ENTRIES];
    char details[DESK_FILE_ENTRIES][32];
    struct desk_file_job job;
    char status[160],saved_path[DESK_FILE_PATH],pending_path[DESK_FILE_PATH];
    char saved_data[DESK_FILE_BYTES+1],clipboard[DESK_FILE_BYTES+1];
    size_t saved_length;
    struct desk_clipboard_client clip;
    unsigned clip_action,clip_target;
    uint64_t clip_revision;
    size_t clip_caret,clip_anchor;
    uint32_t saved_crc,overwrite_crc;
    uint64_t refresh_tick;
};
void tool_clipboard_action(struct tool *tool,struct desk_action action);
void tool_clipboard_result(struct tool *tool,int result);
void tool_status(struct tool *tool,const char *message);
int tool_modified(const struct tool *tool);
void tool_dialog(struct tool *tool,int mode);
void tool_layout(struct tool *tool);
void tool_text_init(struct tool *tool,const char *path);
void tool_text_action(struct tool *tool,struct desk_action action);
void tool_text_finish(struct tool *tool,int result);
void tool_text_request(struct tool *tool,int pending);
void tool_text_dialog(struct tool *tool,unsigned id);
void tool_files_init(struct tool *tool,const char *path);
void tool_files_action(struct tool *tool,struct desk_action action);
void tool_files_finish(struct tool *tool,int result);
void tool_launch(struct tool *tool,int application,const char *path);
#endif
