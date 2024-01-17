#include <renderer.h>
#include <whereami.h>
#include <fast_obj.h>

GLenum glCheckError_(const char *file, int line){
	GLenum errorCode;
	while ((errorCode = glGetError()) != GL_NO_ERROR){
		char *error;
		switch (errorCode){
			case GL_INVALID_ENUM:      error = "INVALID_ENUM"; break;
			case GL_INVALID_VALUE:     error = "INVALID_VALUE"; break;
			case GL_INVALID_OPERATION: error = "INVALID_OPERATION"; break;
			case GL_OUT_OF_MEMORY:     error = "OUT_OF_MEMORY"; break;
			default: error = "UNKNOWN TYPE BEAT";break;
		}
		fatal_error("%s %s (%d)",error,file,line);
	}
	return errorCode;
}

static void check_shader(char *name, GLuint id){
	GLint result;
	glGetShaderiv(id,GL_COMPILE_STATUS,&result);
	if (!result){
		char infolog[512];
		glGetShaderInfoLog(id,512,NULL,infolog);
		fatal_error("Shader compile error in: %s\nLog:\n%s",name,infolog);
	}
}

static void check_program(char *name, char *status_name, GLuint id, GLenum param){
	GLint result;
	glGetProgramiv(id,param,&result);
	if (!result){
		char infolog[512];
		glGetProgramInfoLog(id,512,NULL,infolog);
		fatal_error("Shader program error in: %s\nStatus type: %s\n Log: %s",name,status_name,infolog);
	}
}

static GLuint load_shader(char *name){
	char *vSrc = load_file_as_cstring("res/shaders/%s/vs.glsl",name);
	char *fSrc = load_file_as_cstring("res/shaders/%s/fs.glsl",name);

	GLuint v = glCreateShader(GL_VERTEX_SHADER);
	GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(v,1,&vSrc,NULL);
	glShaderSource(f,1,&fSrc,NULL);

	free(vSrc);
	free(fSrc);

	glCompileShader(v);
	check_shader(format_string("%s/vs.glsl",name),v);
	glCompileShader(f);
	check_shader(format_string("%s/fs.glsl",name),f);
	GLuint p = glCreateProgram();
	glAttachShader(p,v);
	glAttachShader(p,f);
	glLinkProgram(p);
	check_program(name,"link",p,GL_LINK_STATUS);
	glDeleteShader(v);
	glDeleteShader(f);

	return p;
}

static GLuint load_texture(char *name){
	char *path = local_path_to_absolute("res/textures/%s.png",name);
	stbi_set_flip_vertically_on_load(true);
	int width, height, comp;
	unsigned char *pixels = stbi_load(path,&width,&height,&comp,4);
	if (!pixels){
		fatal_error("Failed to load texture:\n%s",path);
	}
	GLuint id;
	glGenTextures(1,&id);
	glBindTexture(GL_TEXTURE_2D,id);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
	free(pixels);
	return id;
}

static GLuint load_cubemap(char *name){
	GLuint id;
	glGenTextures(1,&id);
	glBindTexture(GL_TEXTURE_CUBE_MAP,id);
	stbi_set_flip_vertically_on_load(true);
	const char *faces[] = {
		"px","py","pz",
		"nx","ny","nz",
	};
	for(int i = 0; i < COUNT(faces); i++){
		char *path = local_path_to_absolute("res/cubemaps/%s/%s.png",name,faces[i]);
		int width, height, comp;
		unsigned char *pixels = stbi_load(path,&width,&height,&comp,4);
		if (!pixels){
			fatal_error("Failed to load texture:\n%s",path);
		}
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
			0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels
		);
		free(pixels);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	return id;
}

static void load_model(GPUMesh *mesh, char *name){
	char *path = local_path_to_absolute("res/models/%s.obj",name);
	fastObjMesh *obj = fast_obj_read(path);
	if (!obj){
		fatal_error("Failed to load model:\n%s",path);
	}
	printf(
		"Model: %s\n"
		"\tposition_count: %d\n"
		"\ttexcoord_count: %d\n"
		"\tnormal_count:   %d\n",
		name,
		obj->position_count,
		obj->texcoord_count,
		obj->normal_count
	);
	fast_obj_destroy(obj);
}

static struct {
	GLuint id;
	GLint 
		uVP,
		uCamPos,
		uSkybox,
		uAmbient,
		uReflectivity;
} checkerShader;

#define GET_UNIFORM(shader,name)\
	shader.name = glGetUniformLocation(shader.id,#name);\
	ASSERT(0 <= shader.name);

void renderer_init(){
	checkerShader.id = load_shader("checker");
	GET_UNIFORM(checkerShader,uVP);
	GET_UNIFORM(checkerShader,uCamPos);
	GET_UNIFORM(checkerShader,uSkybox);
	GET_UNIFORM(checkerShader,uAmbient);
	GET_UNIFORM(checkerShader,uReflectivity);

	load_model(0,"world");
}

void delete_gpu_mesh(GPUMesh *m){
	glDeleteBuffers(1,&m->vbo);
	glDeleteVertexArrays(1,&m->vao);
	m->vao = 0;
	m->vbo = 0;
	m->vertex_count = 0;
}