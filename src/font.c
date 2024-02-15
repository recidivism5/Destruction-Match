#include <font.h>

void load_font(Font *f, char *name){
    ASSERT(TTF_DONE == ttf_load_from_file(local_path_to_absolute("res/fonts/%s.ttf",name),&f->ttf,false));
    ASSERT(f->ttf);
    
    int vcount = 0;
    for (char c = '!'; c < '~'; c++){
        int i = ttf_find_glyph(f->ttf,(uint32_t)c);
        ttf_mesh_t *mesh = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh(f->ttf->glyphs+i,&mesh,TTF_QUALITY_HIGH,TTF_FEATURES_DFLT));
        ASSERT(mesh);
        f->voc[c-'!'].offset = vcount;
        f->voc[c-'!'].count = mesh->nfaces*3;
        vcount += mesh->nfaces * 3;
        ttf_free_mesh(mesh);
    }

    vec2 *v = malloc(vcount * sizeof(*v));
    ASSERT(v);
    vcount = 0;
    for (char c = '!'; c < '~'; c++){
        int i = ttf_find_glyph(f->ttf,(uint32_t)c);
        ttf_mesh_t *mesh = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh(f->ttf->glyphs+i,&mesh,TTF_QUALITY_HIGH,TTF_FEATURES_DFLT));
        ASSERT(mesh);
        for (int j = 0; j < mesh->nfaces; j++){
            v[vcount+0][0] = mesh->vert[mesh->faces[j].v1].x;
            v[vcount+0][1] = mesh->vert[mesh->faces[j].v1].y;
            v[vcount+1][0] = mesh->vert[mesh->faces[j].v2].x;
            v[vcount+1][1] = mesh->vert[mesh->faces[j].v2].y;
            v[vcount+2][0] = mesh->vert[mesh->faces[j].v3].x;
            v[vcount+2][1] = mesh->vert[mesh->faces[j].v3].y;
            vcount += 3;
        }
        ttf_free_mesh(mesh);
    }

    glGenBuffers(1,&f->mesh2d);
    glBindBuffer(GL_ARRAY_BUFFER,f->mesh2d);
    glBufferData(GL_ARRAY_BUFFER,vcount*sizeof(*v),v,GL_STATIC_DRAW);
    free(v);
}