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
} BlinnPhongVertex;

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
	AABB boundingBox;
	GLuint vao, vbo;
} Model;

typedef struct {
	Model *model;
	vec3 position;
} ModelInstance;

GLenum glCheckError_(const char *file, int line);
#define glCheckError() glCheckError_(FILENAME, __LINE__)

void renderer_init();

void render(GLFWwindow *window, double dt);