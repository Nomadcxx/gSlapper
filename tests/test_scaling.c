#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "scaling.h"

static int check(const char *name, int tw, int th, int ow, int oh, float panscan, bool fill,
                 bool stretch, float ex, float ey) {
    float sx = 0.0f, sy = 0.0f;
    compute_quad_scale(tw, th, ow, oh, panscan, fill, stretch, &sx, &sy);
    if (fabsf(sx - ex) > 1e-3f || fabsf(sy - ey) > 1e-3f) {
        fprintf(stderr, "%s: got (%.4f, %.4f), expected (%.4f, %.4f)\n", name, sx, sy, ex, ey);
        return 1;
    }
    return 0;
}

int main(void) {
    int failures = 0;
    failures += check("fill same aspect", 1920, 1080, 2560, 1440, 1.0f, true, false, 1.0f, 1.0f);
    failures += check("fill square", 1000, 1000, 1920, 1080, 1.0f, true, false, 1.0f, 1.7778f);
    failures += check("fit square", 1000, 1000, 1920, 1080, 1.0f, false, false, 0.5625f, 1.0f);
    failures += check("stretch", 1000, 1000, 1920, 1080, 0.8f, false, true, 0.8f, 0.8f);
    failures += check("original half", 960, 540, 1920, 1080, -1.0f, false, false, 0.5f, 0.5f);
    failures += check("original clamp", 100, 100, 1920, 1080, -1.0f, false, false, 0.1f, 0.1f);
    return failures != 0;
}
