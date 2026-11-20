#ifndef TRANSITION_H
#define TRANSITION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TRANSITION_NONE = 0,
    TRANSITION_FADE,
    TRANSITION_LEFT,
    TRANSITION_RIGHT,
    TRANSITION_TOP,
    TRANSITION_BOTTOM,
    TRANSITION_WIPE,
    TRANSITION_WAVE,
    TRANSITION_GROW,
    TRANSITION_CENTER,
    TRANSITION_OUTER,
    TRANSITION_ANY,
    TRANSITION_RANDOM
} transition_type_t;

// Values are shared with the transition fragment shader's u_shape uniform.
typedef enum {
    TRANSITION_SHAPE_FADE = 0,
    TRANSITION_SHAPE_WIPE = 1,
    TRANSITION_SHAPE_WAVE = 2,
    TRANSITION_SHAPE_GROW = 3,
    TRANSITION_SHAPE_OUTER = 4
} transition_shape_t;

// What one transition actually draws, after named types and randomness are resolved.
// angle_deg: direction the edge travels, clockwise with screen y down (0 = left to right).
// pos: circle origin as fractions of the output, origin top-left.
typedef struct {
    transition_shape_t shape;
    float angle_deg;
    float pos_x, pos_y;
} transition_params_t;

bool transition_type_from_name(const char *name, transition_type_t *out);
const char *transition_type_name(transition_type_t type);

// Any finite number of degrees, normalised into [0, 360).
bool transition_parse_angle(const char *s, float *deg_out);

// "x,y" fractions in [0,1], or center/top/bottom/left/right/top-left/top-right/
// bottom-left/bottom-right.
bool transition_parse_pos(const char *s, float *x_out, float *y_out);

// Clamps t to [0,1]; fade is linear, the spatial shapes use smoothstep.
float transition_ease(transition_shape_t shape, float t);

transition_params_t transition_resolve(transition_type_t type, float angle_deg,
                                       float pos_x, float pos_y, uint32_t seed);

#endif // TRANSITION_H
