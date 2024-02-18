#pragma once

#include <glutil.h>
#include <ttf2mesh.h>

typedef struct {
    GLuint v2d, ind2d, v3d, ind3d, exp3d, expind3d;
    VertexOffsetCount voc2d[95], voc3d[95];
    ttf_t *ttf;
} Font;

void load_font(Font *f, char *name);