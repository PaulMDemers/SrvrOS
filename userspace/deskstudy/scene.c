#include "scene.h"

void study_draw(struct desk_canvas *c, const struct study_scene *s) {
    struct desk_rect w=s->window;
    const struct desk_font *font=s->large_text ? &desk_font_20 : &desk_font_16;
    desk_fill(c,(struct desk_rect){0,0,c->width,c->height},0xd6dcd2);
    desk_fill(c,(struct desk_rect){0,0,c->width,44},0xf1f0eb);
    desk_text(c,&desk_font_16,24,12,"SrvrOS   /   Desktop foundations",0x292e2b);
    desk_text(c,&desk_font_16,24,c->height-32,"Tab: text size     Esc: return to console",0x465348);
    desk_fill(c,(struct desk_rect){w.x+5,w.y+7,w.w,w.h},0xb9c2b4);
    desk_fill(c,w,0x91a18a);
    desk_fill(c,(struct desk_rect){w.x+1,w.y+1,w.w-2,w.h-2},0xfaf9f6);
    desk_fill(c,(struct desk_rect){w.x+1,w.y+1,w.w-2,42},0xf1f0eb);
    desk_text(c,&desk_font_16,w.x+18,w.y+12,"A quieter desktop",0x292e2b);
    desk_text(c,&desk_font_16,w.x+w.w-30,w.y+12,"×",0x646b65);
    desk_fill(c,(struct desk_rect){w.x+1,w.y+43,w.w-2,1},0xd9ddd5);
    struct desk_rect previous=c->clip;
    desk_clip(c,desk_intersect(previous,(struct desk_rect){w.x+20,w.y+54,w.w-40,w.h-140}));
    desk_text(c,&desk_font_20,w.x+24,w.y+62,"Clear type. Useful space.",0x292e2b);
    desk_text(c,font,w.x+24,w.y+108,
        "The quick brown fox jumps over the lazy dog.\n"
        "Aa Bb Cc Dd Ee Ff Gg  0123456789\n"
        "Café · naïve · £ € — UTF-8 fallback: ☃\n"
        "\nType below to exercise mixed-case input:",0x292e2b);
    desk_clip(c,desk_intersect(previous,(struct desk_rect){w.x+20,w.y+w.h-79,w.w-40,38}));
    desk_fill(c,c->clip,0xe0eade);
    desk_text(c,font,w.x+24,w.y+w.h-74,s->input,0x38644f);
    c->clip=previous;
    desk_fill(c,(struct desk_rect){w.x+1,w.y+w.h-32,w.w-2,31},0xf1f0eb);
    desk_clip(c,desk_intersect(previous,(struct desk_rect){w.x+8,w.y+w.h-31,w.w-16,30}));
    desk_text(c,&desk_font_16,w.x+18,w.y+w.h-27,"Native rendering study · No files are changed",0x646b65);
    c->clip=previous;
    for (int i=0;i<10;i++)
        desk_fill(c,(struct desk_rect){w.x+w.w-5-i,w.y+w.h-4,1,1},0x646b65);
    /* Cursor is part of the same RAM composition and damage accounting. */
    for (int i=0;i<16;i++) {
        desk_fill(c,(struct desk_rect){s->cursor_x,s->cursor_y+i,1+i/2,1},0x292e2b);
        if (i>1 && i<13) desk_fill(c,(struct desk_rect){s->cursor_x+1,s->cursor_y+i,i/2,1},0xfaf9f6);
    }
}
