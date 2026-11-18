#include "scaling.h"

void compute_quad_scale(int tex_w, int tex_h, int out_w, int out_h, float panscan,
                        bool fill, bool stretch, float *scale_x, float *scale_y) {
    float sx, sy;
    float video_aspect = (float)tex_w / (float)tex_h;
    float display_aspect = (float)out_w / (float)out_h;

    if (panscan == -1.0f) {
        // Original resolution: one image pixel per output pixel, clipped if larger
        sx = (float)tex_w / (float)out_w;
        sy = (float)tex_h / (float)out_h;
    } else if (fill) {
        // Cover the output, cropping the excess
        sx = 1.0f;
        sy = 1.0f;
        if (video_aspect > display_aspect)
            sx = video_aspect / display_aspect;
        else
            sy = display_aspect / video_aspect;
    } else if (stretch) {
        sx = panscan;
        sy = panscan;
    } else {
        // Fit inside the output, letterboxed or pillarboxed
        sx = panscan;
        sy = panscan;
        if (video_aspect > display_aspect)
            sy *= display_aspect / video_aspect;
        else
            sx *= video_aspect / display_aspect;
    }

    if (sx < 0.1f) sx = 0.1f;
    if (sx > 10.0f) sx = 10.0f;
    if (sy < 0.1f) sy = 0.1f;
    if (sy > 10.0f) sy = 10.0f;

    *scale_x = sx;
    *scale_y = sy;
}
