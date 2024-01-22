#pragma once

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <base.h>
#include <aabb.h>

typedef struct {
	vec3 position;
	vec3 normal;
	vec2 texcoord;
} PhongVertex;

typedef struct {
	int vertexOffset;
	int vertexCount;
	GLuint textureId;
	int glass;
	float roughness;
} Material;

typedef struct {
	int vertexCount;
	int materialCount;
	Material *materials;
	MMBB boundingBox;
	GLuint vao, vbo;
} Model;

typedef struct {
	Model *model;
	vec3 position;
} ModelInstance;

GLenum glCheckError_(const char *file, int line);
#define glCheckError() glCheckError_(FILENAME, __LINE__)

GLuint load_shader(char *name);

GLuint new_texture(unsigned char *pixels, int width, int height);

GLuint load_texture(char *name);

void delete_texture(GLuint id);

GLuint load_cubemap(char *name);

void load_model(Model *model, char *name);

void delete_model(Model *model);

void shader_set_mat4(GLuint id, char *name, float *mat, bool transpose);

void shader_set_int(GLuint id, char *name, int val);

void shader_set_float(GLuint id, char *name, float val);

void shader_set_vec3(GLuint id, char *name, vec3 v);