#include <matrix_stack.h>
#include <base.h>

static mat4 stack[32];

static int index;

static mat4 mat, temp;

static void mul(){
    glm_mat4_mul(mat,temp,mat);
}

void ms_mul(mat4 m){
    glm_mat4_mul(mat,m,mat);
}

void ms_push(){
    ASSERT(index < (sizeof(stack)/sizeof(*stack)));
    glm_mat4_copy(mat,stack[index]);
    index++;
}

void ms_pop(){
    ASSERT(index > 0);
    glm_mat4_copy(stack[index-1],mat);
    index--;
}

void ms_load_identity(){
	glm_mat4_identity(mat);
}

void ms_euler(vec3 e){
    glm_euler_yxz(e,temp);
    mul();
}

void ms_inverse_euler(vec3 e){
    glm_euler_yxz(e,temp);
    glm_mat4_transpose(temp);
    mul();
}

void ms_trans(vec3 t){
    glm_translate_make(temp,t);
	mul();
}

void ms_ortho(float left, float right, float bottom, float top, float nearZ, float farZ){
	glm_ortho(left,right,bottom,top,nearZ,farZ,temp);
	mul();
}

void ms_persp(float fovy, float aspect, float nearZ, float farZ){
	glm_perspective(fovy,aspect,nearZ,farZ,temp);
	mul();
}

float *ms_get(){
    return (float *)mat;
}