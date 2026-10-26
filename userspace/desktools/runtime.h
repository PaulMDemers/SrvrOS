#ifndef SRVROS_TOOL_RUNTIME_H
#define SRVROS_TOOL_RUNTIME_H
#include "app.h"
void tool_runtime_init(struct tool *tool,int role);
void tool_runtime_event(struct tool *tool,struct desk_packet event);
int tool_runtime_file_poll(struct tool *tool,uint64_t *logged_job);
int tool_file_pool_retire(struct desk_file_pool *pool,int role);
void tool_runtime_draw(struct tool *tool);
int tool_text_runtime(const char *path);
int tool_window_runtime(int role,const char *path);
void tool_runtime_cancel_modal(struct tool *tool);
void tool_runtime_close_request(struct tool *tool);
#endif
