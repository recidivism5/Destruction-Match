#pragma once

#include <cglm/cglm.h>

typedef struct {
    vec3 position, halfExtents;
} AABB;

typedef struct {
    vec3 min, max;
} MMBB;