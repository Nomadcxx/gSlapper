#include <math.h>
#include <stdio.h>
#include <string.h>

#include "transition.h"

static int failures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            failures++;                                                 \
        }                                                               \
    } while (0)

static int near(float a, float b) {
    return fabsf(a - b) < 1e-4f;
}

static void test_names_round_trip(void) {
    const char *names[] = {"none", "fade", "left", "right", "top", "bottom", "wipe",
                           "wave", "grow", "center", "outer", "any", "random"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        transition_type_t type;
        EXPECT(transition_type_from_name(names[i], &type));
        EXPECT(strcmp(transition_type_name(type), names[i]) == 0);
    }
    transition_type_t type;
    EXPECT(!transition_type_from_name("sparkle", &type));
    EXPECT(!transition_type_from_name("", &type));
}

static void test_parse_pos(void) {
    float x, y;
    EXPECT(transition_parse_pos("center", &x, &y) && near(x, 0.5f) && near(y, 0.5f));
    EXPECT(transition_parse_pos("top-left", &x, &y) && near(x, 0.0f) && near(y, 0.0f));
    EXPECT(transition_parse_pos("bottom-right", &x, &y) && near(x, 1.0f) && near(y, 1.0f));
    EXPECT(transition_parse_pos("top", &x, &y) && near(x, 0.5f) && near(y, 0.0f));
    EXPECT(transition_parse_pos("0.25,0.75", &x, &y) && near(x, 0.25f) && near(y, 0.75f));
    EXPECT(!transition_parse_pos("1.5,0", &x, &y));
    EXPECT(!transition_parse_pos("abc", &x, &y));
    EXPECT(!transition_parse_pos("0.5", &x, &y));
    EXPECT(!transition_parse_pos("0.5,0.5x", &x, &y));
}

static void test_parse_angle(void) {
    float deg;
    EXPECT(transition_parse_angle("45", &deg) && near(deg, 45.0f));
    EXPECT(transition_parse_angle("-90", &deg) && near(deg, 270.0f));
    EXPECT(transition_parse_angle("450", &deg) && near(deg, 90.0f));
    EXPECT(transition_parse_angle("360", &deg) && near(deg, 0.0f));
    EXPECT(!transition_parse_angle("nan", &deg));
    EXPECT(!transition_parse_angle("inf", &deg));
    EXPECT(!transition_parse_angle("x", &deg));
    EXPECT(!transition_parse_angle("", &deg));
}

static void test_ease(void) {
    EXPECT(near(transition_ease(TRANSITION_SHAPE_FADE, 0.25f), 0.25f));
    EXPECT(near(transition_ease(TRANSITION_SHAPE_WIPE, 0.5f), 0.5f));
    EXPECT(near(transition_ease(TRANSITION_SHAPE_WIPE, 0.25f), 0.15625f));
    EXPECT(near(transition_ease(TRANSITION_SHAPE_GROW, 2.0f), 1.0f));
    EXPECT(near(transition_ease(TRANSITION_SHAPE_FADE, -1.0f), 0.0f));
}

static void test_resolve(void) {
    transition_params_t p = transition_resolve(TRANSITION_TOP, 45.0f, 0.5f, 0.5f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_WIPE && near(p.angle_deg, 90.0f));
    p = transition_resolve(TRANSITION_LEFT, 45.0f, 0.5f, 0.5f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_WIPE && near(p.angle_deg, 0.0f));
    p = transition_resolve(TRANSITION_RIGHT, 45.0f, 0.5f, 0.5f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_WIPE && near(p.angle_deg, 180.0f));
    p = transition_resolve(TRANSITION_BOTTOM, 45.0f, 0.5f, 0.5f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_WIPE && near(p.angle_deg, 270.0f));
    p = transition_resolve(TRANSITION_WIPE, 30.0f, 0.5f, 0.5f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_WIPE && near(p.angle_deg, 30.0f));
    p = transition_resolve(TRANSITION_WAVE, 30.0f, 0.5f, 0.5f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_WAVE && near(p.angle_deg, 30.0f));
    p = transition_resolve(TRANSITION_GROW, 45.0f, 0.1f, 0.9f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_GROW && near(p.pos_x, 0.1f) && near(p.pos_y, 0.9f));
    p = transition_resolve(TRANSITION_CENTER, 45.0f, 0.1f, 0.9f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_GROW && near(p.pos_x, 0.5f) && near(p.pos_y, 0.5f));
    p = transition_resolve(TRANSITION_OUTER, 45.0f, 0.2f, 0.3f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_OUTER && near(p.pos_x, 0.2f) && near(p.pos_y, 0.3f));
    p = transition_resolve(TRANSITION_FADE, 45.0f, 0.5f, 0.5f, 7);
    EXPECT(p.shape == TRANSITION_SHAPE_FADE);

    int seen[5] = {0};
    for (uint32_t seed = 1; seed <= 1000; seed++) {
        p = transition_resolve(TRANSITION_RANDOM, 45.0f, 0.5f, 0.5f, seed);
        EXPECT(p.shape >= TRANSITION_SHAPE_FADE && p.shape <= TRANSITION_SHAPE_OUTER);
        if (p.shape >= TRANSITION_SHAPE_FADE && p.shape <= TRANSITION_SHAPE_OUTER)
            seen[p.shape] = 1;
        p = transition_resolve(TRANSITION_ANY, 45.0f, 0.5f, 0.5f, seed);
        EXPECT(p.shape == TRANSITION_SHAPE_GROW);
        EXPECT(p.pos_x >= 0.0f && p.pos_x <= 1.0f && p.pos_y >= 0.0f && p.pos_y <= 1.0f);
    }
    for (int i = 0; i < 5; i++)
        EXPECT(seen[i]);
}

int main(void) {
    test_names_round_trip();
    test_parse_pos();
    test_parse_angle();
    test_ease();
    test_resolve();
    return failures != 0;
}
