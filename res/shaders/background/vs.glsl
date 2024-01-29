#version 330 core

//from https://stackoverflow.com/a/59738005/15210335

out vec2 vTexCoord;

void main() {
    const vec2 positions[4] = vec2[](
        vec2(-1, -1),
        vec2(+1, -1),
        vec2(-1, +1),
        vec2(+1, +1)
    );
    const vec2 coords[4] = vec2[](
        vec2(0, 0),
        vec2(1, 0),
        vec2(0, 1),
        vec2(1, 1)
    );

    vTexCoord = coords[gl_VertexID];
    gl_Position = vec4(positions[gl_VertexID], 1.0, 1.0);
}