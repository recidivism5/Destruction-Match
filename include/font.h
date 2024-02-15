#pragma once

#include <glutil.h>
#include <ttf2mesh.h>

typedef struct {
    GLuint mesh2d, mesh3d;
    VertexOffsetCount voc2d[95], voc3d[95];
    ttf_t *ttf;
} Font;

void load_font(Font *f, char *name);