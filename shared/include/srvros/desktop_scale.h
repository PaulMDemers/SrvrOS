#ifndef SRVROS_DESKTOP_SCALE_H
#define SRVROS_DESKTOP_SCALE_H

/* Keep the established 1 and 2 values on disk and on the wire. Fractional
 * output scales use percentages. Client surfaces keep integer density. */
enum { DESK_SCALE_125 = 125, DESK_SCALE_150 = 150, DESK_SCALE_175 = 175 };

static inline int desk_scale_fractional(unsigned scale) {
    return scale==DESK_SCALE_125 || scale==DESK_SCALE_150 || scale==DESK_SCALE_175;
}
static inline int desk_scale_valid(unsigned scale) {
    return scale==1 || scale==2 || desk_scale_fractional(scale);
}
static inline unsigned desk_scale_quarters(unsigned scale) {
    return desk_scale_fractional(scale)?scale/25:scale*4;
}
static inline unsigned desk_scale_density(unsigned scale) { return scale==1?1:2; }
static inline unsigned desk_scale_logical(unsigned physical,unsigned scale) {
    unsigned quarters=desk_scale_quarters(scale);
    return (physical*4+quarters-1)/quarters;
}
static inline const char *desk_scale_label(unsigned scale) {
    switch(scale) {
    case DESK_SCALE_125: return "1.25x";
    case DESK_SCALE_150: return "1.5x";
    case DESK_SCALE_175: return "1.75x";
    case 2: return "2x";
    default: return "1x";
    }
}

#endif
