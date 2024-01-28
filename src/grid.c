#include <grid.h>

void gen_rounded_grid(GPUMesh *mesh, int width, int height, float lineWidth){
    /*cells will be 1x1 internally
    square at each corner, takes 6 verts
    rectangle for each edge, takes 6 verts
*/
    mesh->vertexCount = (width*(height+1) + height*(width+1) + (width+1)*(height+1)) * 6;
    vec2 *verts = malloc(mesh->vertexCount * sizeof(*verts));
    ASSERT(verts);
    vec2 *v = verts;
    float step = lineWidth + 1.0f;
    for (int y = 0; y < height+1; y++){
        for (int x = 0; x < width+1; x++){
            float fx = (float)x * step;
            float fy = (float)y * step;

            v[0][0] = fx;
            v[0][1] = fy;

            v[1][0] = fx+lineWidth;
            v[1][1] = fy;

            v[2][0] = fx+lineWidth;
            v[2][1] = fy+lineWidth;

            v[3][0] = fx+lineWidth;
            v[3][1] = fy+lineWidth;

            v[4][0] = fx;
            v[4][1] = fy+lineWidth;

            v[5][0] = fx;
            v[5][1] = fy;

            v += 6;
        }
    }
    for (int y = 0; y < height+1; y++){
        for (int x = 0; x < width; x++){
            float fx = (float)x * step + lineWidth;
            float fy = (float)y * step;

            v[0][0] = fx;
            v[0][1] = fy;

            v[1][0] = fx+1.0f;
            v[1][1] = fy;

            v[2][0] = fx+1.0f;
            v[2][1] = fy+lineWidth;

            v[3][0] = fx+1.0f;
            v[3][1] = fy+lineWidth;

            v[4][0] = fx;
            v[4][1] = fy+lineWidth;

            v[5][0] = fx;
            v[5][1] = fy;

            v += 6;
        }
    }
    for (int x = 0; x < width+1; x++){
        for (int y = 0; y < height; y++){
            float fx = (float)x * step;
            float fy = (float)y * step + lineWidth;

            v[0][0] = fx;
            v[0][1] = fy;

            v[1][0] = fx+lineWidth;
            v[1][1] = fy;

            v[2][0] = fx+lineWidth;
            v[2][1] = fy+1.0f;

            v[3][0] = fx+lineWidth;
            v[3][1] = fy+1.0f;

            v[4][0] = fx;
            v[4][1] = fy+1.0f;

            v[5][0] = fx;
            v[5][1] = fy;

            v += 6;
        }
    }

    ASSERT(v == verts+mesh->vertexCount);
    
    glGenVertexArrays(1,&mesh->vao);
    glBindVertexArray(mesh->vao);
    glGenBuffers(1,&mesh->vbo);
    glBindBuffer(GL_ARRAY_BUFFER,mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER,mesh->vertexCount*sizeof(*verts),verts,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(*verts),(void *)0);

    free(verts);
}