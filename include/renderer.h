#pragma once

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <cglm/cglm.h>

#include <base.h>

TSTRUCT(Texture){
	GLuint id;
	int width, height;
};

TSTRUCT(GPUMesh){
	GLuint vao, vbo;
	int vertex_count;
};

GLenum glCheckError_(const char *file, int line);
#define glCheckError() glCheckError_(__FILE__, __LINE__)

void load_texture(Texture *t, char *name);

GLuint load_shader(char *name);