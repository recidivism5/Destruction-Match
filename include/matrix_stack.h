#pragma once

#include <cglm/cglm.h>

void ms_mul(mat4 m);

void ms_push();

void ms_pop();

void ms_load_identity();

void ms_euler(vec3 e);

void ms_inverse_euler(vec3 e);

void ms_trans(vec3 t);

void ms_ortho(float left, float right, float bottom, float top, float nearZ, float farZ);

void ms_persp(float fovy, float aspect, float nearZ, float farZ);

float *ms_get();