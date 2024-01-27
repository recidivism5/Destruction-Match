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
	GLuint textureId;
	float roughness;
} Material;

typedef struct {
	int offset, count;
} VertexOffsetCount;

typedef struct {
	vec3 position;
	VertexOffsetCount *vertexOffsetCounts;
} FracturedObject;

typedef struct {
	int vertexCount;
	GLuint vao, vbo;
	int materialCount;
	Material *materials;
	int objectCount;
	FracturedObject *objects;
} FracturedModel;

typedef struct {
	vec3 position;
	vec3 velocity;
	vec3 angularVelocity;
	vec4 quaternion;
} BodyData;

typedef struct {
	FracturedModel *model;
	BodyData *bodyDatas;
} FracturedModelInstance;

GLenum glCheckError_(const char *file, int line);
#define glCheckError() glCheckError_(FILENAME, __LINE__)

GLuint load_shader(char *name);

GLuint new_texture(unsigned char *pixels, int width, int height);

GLuint load_texture(char *name);

void delete_texture(GLuint id);

GLuint load_cubemap(char *name);

void load_fractured_model(FracturedModel *model, char *name);

void delete_fractured_model(FracturedModel *model);

void shader_set_mat4(GLuint id, char *name, float *mat, bool transpose);

void shader_set_int(GLuint id, char *name, int val);

void shader_set_float(GLuint id, char *name, float val);

void shader_set_vec3(GLuint id, char *name, vec3 v);