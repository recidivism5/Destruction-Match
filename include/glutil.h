#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <linmath.h>
#include <stb_image.h>
#include <ttf2mesh.h>

#include <base.h>

typedef struct {
	int width, height;
	GLuint id;
} Texture;

typedef struct {
	int offset, count;
} VertexOffsetCount;

typedef struct {
	vec3 position;
	vec3 normal;
} NormalVertex;

GLenum glCheckError_(const char *file, int line);
#define glCheckError() glCheckError_(FILENAME, __LINE__)

GLuint load_shader(char *name);

GLuint new_texture(unsigned char *pixels, int width, int height, bool interpolated);

void load_texture(Texture *t, char *name, bool interpolated);

void delete_texture(GLuint id);

void project_identity();

void project_ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);

void project_perspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);