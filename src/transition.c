#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "transition.h"

static const char *const type_names[] = {
    [TRANSITION_NONE] = "none",
    [TRANSITION_FADE] = "fade",
    [TRANSITION_LEFT] = "left",
    [TRANSITION_RIGHT] = "right",
    [TRANSITION_TOP] = "top",
    [TRANSITION_BOTTOM] = "bottom",
    [TRANSITION_WIPE] = "wipe",
    [TRANSITION_WAVE] = "wave",
    [TRANSITION_GROW] = "grow",
    [TRANSITION_CENTER] = "center",
    [TRANSITION_OUTER] = "outer",
    [TRANSITION_ANY] = "any",
    [TRANSITION_RANDOM] = "random",
};

#define TYPE_COUNT (sizeof(type_names) / sizeof(type_names[0]))

static const struct {
    const char *name;
    float x, y;
} named_positions[] = {
    {"center", 0.5f, 0.5f},
    {"top", 0.5f, 0.0f},
    {"bottom", 0.5f, 1.0f},
    {"left", 0.0f, 0.5f},
    {"right", 1.0f, 0.5f},
    {"top-left", 0.0f, 0.0f},
    {"top-right", 1.0f, 0.0f},
    {"bottom-left", 0.0f, 1.0f},
    {"bottom-right", 1.0f, 1.0f},
};

bool transition_type_from_name(const char *name, transition_type_t *out) {
    if (!name)
        return false;
    for (size_t i = 0; i < TYPE_COUNT; i++) {
        if (strcmp(name, type_names[i]) == 0) {
            *out = (transition_type_t)i;
            return true;
        }
    }
    return false;
}

const char *transition_type_name(transition_type_t type) {
    return (size_t)type < TYPE_COUNT ? type_names[type] : "none";
}

// Parses the whole string as one finite float
static bool parse_float(const char *s, const char **end_out, float *value_out) {
    char *end;
    float value = strtof(s, &end);
    if (end == s || !isfinite(value))
        return false;
    *end_out = end;
    *value_out = value;
    return true;
}

bool transition_parse_angle(const char *s, float *deg_out) {
    const char *end;
    float deg;
    if (!s || !parse_float(s, &end, &deg) || *end != '\0')
        return false;
    deg = fmodf(deg, 360.0f);
    if (deg < 0.0f)
        deg += 360.0f;
    *deg_out = deg;
    return true;
}

bool transition_parse_pos(const char *s, float *x_out, float *y_out) {
    if (!s)
        return false;
    for (size_t i = 0; i < sizeof(named_positions) / sizeof(named_positions[0]); i++) {
        if (strcmp(s, named_positions[i].name) == 0) {
            *x_out = named_positions[i].x;
            *y_out = named_positions[i].y;
            return true;
        }
    }

    const char *end;
    float x, y;
    if (!parse_float(s, &end, &x) || *end != ',')
        return false;
    if (!parse_float(end + 1, &end, &y) || *end != '\0')
        return false;
    if (x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f)
        return false;
    *x_out = x;
    *y_out = y;
    return true;
}

float transition_ease(transition_shape_t shape, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (shape == TRANSITION_SHAPE_FADE)
        return t;
    return t * t * (3.0f - 2.0f * t);
}

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

static float random_unit(uint32_t *state) {
    return (float)(xorshift32(state) >> 8) / (float)(1u << 24);
}

transition_params_t transition_resolve(transition_type_t type, float angle_deg,
                                       float pos_x, float pos_y, uint32_t seed) {
    uint32_t state = seed | 1u;
    transition_params_t p = {TRANSITION_SHAPE_FADE, angle_deg, pos_x, pos_y};

    if (type == TRANSITION_RANDOM) {
        int span = TRANSITION_ANY - TRANSITION_FADE + 1;
        type = (transition_type_t)(TRANSITION_FADE + (int)(xorshift32(&state) % (uint32_t)span));
    }

    switch (type) {
        case TRANSITION_LEFT:   p.shape = TRANSITION_SHAPE_WIPE; p.angle_deg = 0.0f; break;
        case TRANSITION_TOP:    p.shape = TRANSITION_SHAPE_WIPE; p.angle_deg = 90.0f; break;
        case TRANSITION_RIGHT:  p.shape = TRANSITION_SHAPE_WIPE; p.angle_deg = 180.0f; break;
        case TRANSITION_BOTTOM: p.shape = TRANSITION_SHAPE_WIPE; p.angle_deg = 270.0f; break;
        case TRANSITION_WIPE:   p.shape = TRANSITION_SHAPE_WIPE; break;
        case TRANSITION_WAVE:   p.shape = TRANSITION_SHAPE_WAVE; break;
        case TRANSITION_GROW:   p.shape = TRANSITION_SHAPE_GROW; break;
        case TRANSITION_OUTER:  p.shape = TRANSITION_SHAPE_OUTER; break;
        case TRANSITION_CENTER:
            p.shape = TRANSITION_SHAPE_GROW;
            p.pos_x = 0.5f;
            p.pos_y = 0.5f;
            break;
        case TRANSITION_ANY:
            p.shape = TRANSITION_SHAPE_GROW;
            p.pos_x = random_unit(&state);
            p.pos_y = random_unit(&state);
            break;
        default:
            p.shape = TRANSITION_SHAPE_FADE;
            break;
    }
    return p;
}
