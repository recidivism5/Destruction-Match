#include <font.h>

void load_font(Font *f, char *name){
    int quality = TTF_QUALITY_NORMAL;

    ASSERT(TTF_DONE == ttf_load_from_file(local_path_to_absolute("res/fonts/%s.ttf",name),&f->ttf,false));
    ASSERT(f->ttf);
    
    int nv2d = 0;
    int nind2d = 0;
    int nv3d = 0;
    int nexp3d = 0;
    int nind3d = 0;
    for (char c = '!'; c < '~'; c++){
        int i = ttf_find_glyph(f->ttf,(uint32_t)c);
        ASSERT(i >= 0);
        ttf_glyph_t *g = f->ttf->glyphs+i;

        ttf_mesh_t *mesh2d = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh(g,&mesh2d,quality,TTF_FEATURES_DFLT));
        ASSERT(mesh2d);
        f->voc2d[c-'!'].offset = nind2d;
        f->voc2d[c-'!'].count = mesh2d->nfaces*3;
        nind2d += mesh2d->nfaces*3;
        nv2d += mesh2d->nvert;
        ttf_free_mesh(mesh2d);

        ttf_mesh3d_t *mesh3d = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh3d(g,&mesh3d,quality,TTF_FEATURES_DFLT,1.0f));
        ASSERT(mesh3d);
        f->voc3d[c-'!'].offset = nind3d;
        f->voc3d[c-'!'].count = mesh3d->nfaces*3;
        nind3d += mesh3d->nfaces*3;
        nv3d += mesh3d->nvert;
        nexp3d += mesh3d->nvert/3;
        ttf_free_mesh3d(mesh3d);
    }
    vec2 *v2d = malloc(nv2d * sizeof(*v2d));
    ASSERT(v2d);
    int *ind2d = malloc(nind2d * sizeof(*ind2d));
    ASSERT(ind2d);
    NormalVertex *v3d = malloc(nv3d * sizeof(*v3d));
    ASSERT(v3d);
    vec3 *exp3d = malloc(nv3d * sizeof(*exp3d));
    ASSERT(exp3d);
    int *ind3d = malloc(nind3d * sizeof(*ind3d));
    ASSERT(ind3d);
    int *expind3d = malloc(nind3d * sizeof(*expind3d));
    ASSERT(expind3d);
    nv2d = 0;
    nind2d = 0;
    nv3d = 0;
    nexp3d = 0;
    nind3d = 0;
    for (char c = '!'; c < '~'; c++){
        int i = ttf_find_glyph(f->ttf,(uint32_t)c);
        ASSERT(i >= 0);
        ttf_glyph_t *g = f->ttf->glyphs+i;

        ttf_mesh_t *mesh2d = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh(g,&mesh2d,quality,TTF_FEATURES_DFLT));
        ASSERT(mesh2d);
        for (int j = 0; j < mesh2d->nvert; j++){
            v2d[nv2d+j][0] = mesh2d->vert[j].x;
            v2d[nv2d+j][1] = mesh2d->vert[j].y;
        }
        for (int j = 0; j < mesh2d->nfaces; j++){
            int *f = &mesh2d->faces[j].v1;
            for (int k = 0; k < 3; k++){
                ASSERT(f[k] >= 0 && f[k] < mesh2d->nvert);
                ind2d[nind2d+j*3+k] = f[k] + nv2d;
            }
        }
        nv2d += mesh2d->nvert;
        nind2d += mesh2d->nfaces*3;
        ttf_free_mesh(mesh2d);

        ttf_mesh3d_t *mesh3d = 0;
        ASSERT(TTF_DONE == ttf_glyph2mesh3d(g,&mesh3d,quality,TTF_FEATURES_DFLT,1.0f));
        ASSERT(mesh3d);
        for (int j = 0; j < mesh3d->nvert; j++){
            vec3_copy(&mesh3d->vert[j].x,v3d[nv3d+j].position);
            vec3_copy(&mesh3d->normals[j].x,v3d[nv3d+j].normal);
        }
        for (int j = 0; j < mesh3d->nfaces; j++){
            int *f = &mesh3d->faces[j].v1;
            for (int k = 0; k < 3; k++){
                ASSERT(f[k] >= 0 && f[k] < mesh3d->nvert);
                ind3d[nind3d+j*3+k] = f[k] + nv3d;
            }
        }
        int uniqueMax = mesh3d->nvert/3;
        vec3 *unique = exp3d + nexp3d;
        int uniqueCount = 0;
        for (int j = 0; j < mesh3d->nvert; j++){
            int k;
            for (k = 0; k < uniqueCount; k++){
                if (vec3_equal(unique[k],&mesh3d->vert[j].x)){
                    goto L0;
                }
            }
            ASSERT(uniqueCount < uniqueMax);
            vec3_copy(&mesh3d->vert[j].x,unique[uniqueCount++]);
            L0:;
        }
        ASSERT(uniqueCount == uniqueMax);
        int *ind = expind3d + nind3d;
        for (int j = 0; j < mesh3d->nfaces; j++){
            int *ip = &mesh3d->faces[j].v1;
            for (int k = 0; k < 3; k++){
                for (int m = 0; m < uniqueCount; m++){
                    if (vec3_equal(&mesh3d->vert[ip[k]].x,unique[m])){
                        ind[j*3+k] = m + nexp3d;
                        goto L1;
                    }
                }
                ASSERT(0 && "vertex not found");
                L1:;
            }
        }
        for (int j = 0; j < uniqueCount; j++){
            vec3 n = {0,0,0};
            for (int k = 0; k < mesh3d->nfaces; k++){
                int *ip = &mesh3d->faces[k].v1;
                for (int m = 0; m < 3; m++){
                    if (vec3_equal(&mesh3d->vert[ip[m]].x,unique[j])){
                        vec3_add(n,&mesh3d->normals[ip[m]].x,n);
                        break;
                    }
                }
            }
            n[2] = 0.0f;
            vec3_set_length(n,0.05f,n);
            vec3_add(unique[j],n,unique[j]);
        }

        nv3d += mesh3d->nvert;
        nexp3d += uniqueCount;
        nind3d += mesh3d->nfaces*3;

        ttf_free_mesh3d(mesh3d);
    }

    glGenBuffers(1,&f->v2d);
    glBindBuffer(GL_ARRAY_BUFFER,f->v2d);
    glBufferData(GL_ARRAY_BUFFER,nv2d*sizeof(*v2d),v2d,GL_STATIC_DRAW);
    free(v2d);

    glGenBuffers(1,&f->ind2d);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,f->ind2d);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,nind2d*sizeof(*ind2d),ind2d,GL_STATIC_DRAW);
    free(ind2d);

    glGenBuffers(1,&f->v3d);
    glBindBuffer(GL_ARRAY_BUFFER,f->v3d);
    glBufferData(GL_ARRAY_BUFFER,nv3d*sizeof(*v3d),v3d,GL_STATIC_DRAW);
    free(v3d);

    glGenBuffers(1,&f->ind3d);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,f->ind3d);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,nind3d*sizeof(*ind3d),ind3d,GL_STATIC_DRAW);
    free(ind3d);

    glGenBuffers(1,&f->exp3d);
    glBindBuffer(GL_ARRAY_BUFFER,f->exp3d);
    glBufferData(GL_ARRAY_BUFFER,nexp3d*sizeof(*exp3d),exp3d,GL_STATIC_DRAW);
    free(exp3d);

    glGenBuffers(1,&f->expind3d);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,f->expind3d);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,nind3d*sizeof(*expind3d),expind3d,GL_STATIC_DRAW);
    free(expind3d);
}