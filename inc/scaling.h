#ifndef SCALING_H
#define SCALING_H

#include <stdbool.h>

// Half-extents of the centred quad (in NDC) that shows a tex_w x tex_h image on
// an out_w x out_h output. panscan == -1.0 selects original (1:1) mode; then
// fill, then stretch, otherwise fit with panscan as the scale factor.
void compute_quad_scale(int tex_w, int tex_h, int out_w, int out_h, float panscan,
                        bool fill, bool stretch, float *scale_x, float *scale_y);

#endif // SCALING_H
