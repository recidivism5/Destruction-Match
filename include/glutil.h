#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <linmath.h>
#include <stb_image.h>
#include <stb_truetype.h>

#include <base.h>

typedef struct {
	float left,top,right,bottom;
} FRect;

typedef struct {
	float x,y,width,height;
} FSRect;

typedef struct {
	int width, height;
	GLuint id;
} Texture;

typedef struct {
	GLuint id;
	int width;
	stbtt_bakedchar *bakedChars;
} FontAtlas;

typedef struct {
	vec3 position;
	vec3 normal;
	vec2 texcoord;
} ModelVertex;

typedef struct {
	vec3 position;
	vec2 texcoord;
	vec3 color;
} ScreenVertex;

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
	ModelVertex *vertices;
	vec3 *expandedPositions;
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

GLuint new_texture(unsigned char *pixels, int width, int height, bool interpolated);

void load_texture(Texture *t, char *name, bool interpolated);

void delete_texture(GLuint id);

void gen_font_atlas(FontAtlas *atlas, char *name, int height);

void load_fractured_model(FracturedModel *model, char *name);

void delete_fractured_model(FracturedModel *model);

void project_identity();

void project_ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);

void project_perspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);