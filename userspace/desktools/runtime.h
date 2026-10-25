#ifndef SRVROS_TOOL_RUNTIME_H
#define SRVROS_TOOL_RUNTIME_H
#include "app.h"
void tool_runtime_init(struct tool *tool,int role);
void tool_runtime_event(struct tool *tool,struct desk_packet event);
int tool_runtime_file_poll(struct tool *tool,uint64_t *logged_job);
void tool_runtime_draw(struct tool *tool);
int tool_text_runtime(const char *path);
#endif
