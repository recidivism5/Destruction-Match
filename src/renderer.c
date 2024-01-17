#include <renderer.h>
#include <whereami.h>

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

GLuint load_shader(char *name){
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

void load_texture(Texture *t, char *name){
	int comp;
	stbi_set_flip_vertically_on_load(true);
	char *path = local_path_to_absolute("res/textures/%s.png",name);
	unsigned char *pixels = stbi_load(path,&t->width,&t->height,&comp,4);
	if (!pixels){
		fatal_error("Texture: \"%s\" not found.",path);
	}
	glGenTextures(1,&t->id);
	glBindTexture(GL_TEXTURE_2D,t->id);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,t->width,t->height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
	free(pixels);
}

GLuint load_cubemap(char *name){
	
}

void delete_gpu_mesh(GPUMesh *m){
	glDeleteBuffers(1,&m->vbo);
	glDeleteVertexArrays(1,&m->vao);
	m->vao = 0;
	m->vbo = 0;
	m->vertex_count = 0;
}