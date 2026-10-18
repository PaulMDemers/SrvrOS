#ifndef SRVROS_DESK_FLOW_H
#define SRVROS_DESK_FLOW_H
#include <srvros/desk_render.h>
/* Word wrapping with Unicode boundaries, explicit newlines and character
 * wrapping for long words. Tabs display as four spaces. Drawing preserves clip. */
int desk_text_block_height(const struct desk_font *font,const char *text,int width,int max_lines);
void desk_text_block_draw(struct desk_canvas *canvas,struct desk_rect frame,const struct desk_font *font,const char *text,uint32_t color);
#endif
