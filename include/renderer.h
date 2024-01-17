#pragma once

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <cglm/cglm.h>

#include <base.h>

TSTRUCT(GPUMesh){
	GLuint vao, vbo;
	int vertex_count;
};

TSTRUCT(GPUMeshInstance){
	GPUMesh *mesh;
	vec3 position;
};

GLenum glCheckError_(const char *file, int line);
#define glCheckError() glCheckError_(FILENAME, __LINE__)

void renderer_init();