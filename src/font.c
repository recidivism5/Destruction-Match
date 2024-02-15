#include <font.h>

void load_font(Font *f, char *name){
    ASSERT(TTF_DONE == ttf_load_from_file(local_path_to_absolute("res/fonts/%s.ttf",name),&f->ttf,false));
    ASSERT(f->ttf);
    
    int vcount2d = 0;
    int vcount3d = 0;
    for (char c = '!'; c < '~'; c++){
        int i = ttf_find_glyph(f->ttf,(uint32_t)c);
        ASSERT(i >= 0);
        ttf_glyph_t *g = f->ttf->glyphs+i;

        ttf_mesh_t *mesh2d = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh(g,&mesh2d,TTF_QUALITY_HIGH,TTF_FEATURES_DFLT));
        ASSERT(mesh2d);
        f->voc2d[c-'!'].offset = vcount2d;
        f->voc2d[c-'!'].count = mesh2d->nfaces*3;
        vcount2d += mesh2d->nfaces * 3;
        ttf_free_mesh(mesh2d);

        ttf_mesh3d_t *mesh3d = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh3d(g,&mesh3d,TTF_QUALITY_HIGH,TTF_FEATURES_DFLT,1.0f));
        ASSERT(mesh3d);
        f->voc3d[c-'!'].offset = vcount3d;
        f->voc3d[c-'!'].count = mesh3d->nfaces*3;
        vcount3d += mesh3d->nfaces * 3;
        ttf_free_mesh3d(mesh3d);
    }

    vec2 *v2d = malloc(vcount2d * sizeof(*v2d));
    ASSERT(v2d);
    NormalVertex *v3d = malloc(vcount3d * sizeof(*v3d));
    ASSERT(v3d);
    vcount2d = 0;
    vcount3d = 0;
    for (char c = '!'; c < '~'; c++){
        int i = ttf_find_glyph(f->ttf,(uint32_t)c);
        ASSERT(i >= 0);
        ttf_glyph_t *g = f->ttf->glyphs+i;

        ttf_mesh_t *mesh2d = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh(g,&mesh2d,TTF_QUALITY_HIGH,TTF_FEATURES_DFLT));
        ASSERT(mesh2d);
        for (int j = 0; j < mesh2d->nfaces; j++){
            v2d[vcount2d+0][0] = mesh2d->vert[mesh2d->faces[j].v1].x;
            v2d[vcount2d+0][1] = mesh2d->vert[mesh2d->faces[j].v1].y;
            v2d[vcount2d+1][0] = mesh2d->vert[mesh2d->faces[j].v2].x;
            v2d[vcount2d+1][1] = mesh2d->vert[mesh2d->faces[j].v2].y;
            v2d[vcount2d+2][0] = mesh2d->vert[mesh2d->faces[j].v3].x;
            v2d[vcount2d+2][1] = mesh2d->vert[mesh2d->faces[j].v3].y;
            vcount2d += 3;
        }
        ttf_free_mesh(mesh2d);

        ttf_mesh3d_t *mesh3d = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh3d(g,&mesh3d,TTF_QUALITY_HIGH,TTF_FEATURES_DFLT,1.0f));
        ASSERT(mesh3d);
        for (int j = 0; j < mesh3d->nfaces; j++){
            v3d[vcount3d+0].position[0] = mesh3d->vert[mesh3d->faces[j].v1].x;
            v3d[vcount3d+0].position[1] = mesh3d->vert[mesh3d->faces[j].v1].y;
            v3d[vcount3d+0].position[2] = mesh3d->vert[mesh3d->faces[j].v1].z;
            v3d[vcount3d+0].normal[0] = mesh3d->normals[mesh3d->faces[j].v1].x;
            v3d[vcount3d+0].normal[1] = mesh3d->normals[mesh3d->faces[j].v1].y;
            v3d[vcount3d+0].normal[2] = mesh3d->normals[mesh3d->faces[j].v1].z;
            v3d[vcount3d+1].position[0] = mesh3d->vert[mesh3d->faces[j].v2].x;
            v3d[vcount3d+1].position[1] = mesh3d->vert[mesh3d->faces[j].v2].y;
            v3d[vcount3d+1].position[2] = mesh3d->vert[mesh3d->faces[j].v2].z;
            v3d[vcount3d+1].normal[0] = mesh3d->normals[mesh3d->faces[j].v2].x;
            v3d[vcount3d+1].normal[1] = mesh3d->normals[mesh3d->faces[j].v2].y;
            v3d[vcount3d+1].normal[2] = mesh3d->normals[mesh3d->faces[j].v2].z;
            v3d[vcount3d+2].position[0] = mesh3d->vert[mesh3d->faces[j].v3].x;
            v3d[vcount3d+2].position[1] = mesh3d->vert[mesh3d->faces[j].v3].y;
            v3d[vcount3d+2].position[2] = mesh3d->vert[mesh3d->faces[j].v3].z;
            v3d[vcount3d+2].normal[0] = mesh3d->normals[mesh3d->faces[j].v3].x;
            v3d[vcount3d+2].normal[1] = mesh3d->normals[mesh3d->faces[j].v3].y;
            v3d[vcount3d+2].normal[2] = mesh3d->normals[mesh3d->faces[j].v3].z;
            vcount3d += 3;
        }
        ttf_free_mesh3d(mesh3d);
    }

    glGenBuffers(1,&f->mesh2d);
    glBindBuffer(GL_ARRAY_BUFFER,f->mesh2d);
    glBufferData(GL_ARRAY_BUFFER,vcount2d*sizeof(*v2d),v2d,GL_STATIC_DRAW);
    free(v2d);

    glGenBuffers(1,&f->mesh3d);
    glBindBuffer(GL_ARRAY_BUFFER,f->mesh3d);
    glBufferData(GL_ARRAY_BUFFER,vcount3d*sizeof(*v3d),v3d,GL_STATIC_DRAW);
    free(v3d);
}