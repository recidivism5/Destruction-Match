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

void texture_from_file(Texture *t, char *name){
	int comp;
	stbi_set_flip_vertically_on_load(true);
	char *path = get_resource_path(name,".png");
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

void check_shader(char *name, char *type, GLuint id){
	GLint result;
	glGetShaderiv(id,GL_COMPILE_STATUS,&result);
	if (!result){
		char infolog[512];
		glGetShaderInfoLog(id,512,NULL,infolog);
		fatal_error("%s %s shader compile error: %s",name,type,infolog);
	}
}

void check_program(char *name, char *status_name, GLuint id, GLenum param){
	GLint result;
	glGetProgramiv(id,param,&result);
	if (!result){
		char infolog[512];
		glGetProgramInfoLog(id,512,NULL,infolog);
		fatal_error("%s shader %s error: %s",name,status_name,infolog);
	}
}

GLuint compile_shader(char *name, char *vert_src, char *frag_src){
	GLuint v = glCreateShader(GL_VERTEX_SHADER);
	GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(v,1,&vert_src,NULL);
	glShaderSource(f,1,&frag_src,NULL);
	glCompileShader(v);
	check_shader(name,"vertex",v);
	glCompileShader(f);
	check_shader(name,"fragment",f);
	GLuint p = glCreateProgram();
	glAttachShader(p,v);
	glAttachShader(p,f);
	glLinkProgram(p);
	check_program(name,"link",p,GL_LINK_STATUS);
	glDeleteShader(v);
	glDeleteShader(f);
	return p;
}

#define COMPILE_SHADER(s) s.id = compile_shader(#s,s.vert_src,s.frag_src)
#define GET_ATTRIB(s,a) s.a = glGetAttribLocation(s.id,#a)
#define GET_UNIFORM(s,u) s.u = glGetUniformLocation(s.id,#u)

struct ColorShader color_shader = {
	"#version 330 core\n"
	"in vec3 aPosition;\n"
	"in vec4 aColor;\n"
	"uniform mat4 uMVP;\n"
	"out vec4 vColor;\n"
	"void main(){\n"
	"	gl_Position = uMVP * vec4(aPosition,1.0f);\n"
	"	vColor = aColor;\n"
	"}",

	"#version 330 core\n"
	"in vec4 vColor;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"	FragColor = vColor;\n"
	"}"
};

static void compile_color_shader(){
	COMPILE_SHADER(color_shader);
	GET_ATTRIB(color_shader,aPosition);
	GET_ATTRIB(color_shader,aColor);
	GET_UNIFORM(color_shader,uMVP);
}

void gpu_mesh_from_color_verts(GPUMesh *m, ColorVertex *verts, int count){
	glGenVertexArrays(1,&m->vao);
	glBindVertexArray(m->vao);
	glGenBuffers(1,&m->vbo);
	glBindBuffer(GL_ARRAY_BUFFER,m->vbo);
	glBufferData(GL_ARRAY_BUFFER,count*sizeof(*verts),verts,GL_STATIC_DRAW);
	glEnableVertexAttribArray(color_shader.aPosition);
	glEnableVertexAttribArray(color_shader.aColor);
	glVertexAttribPointer(color_shader.aPosition,3,GL_FLOAT,GL_FALSE,sizeof(ColorVertex),(void *)0);
	glVertexAttribPointer(color_shader.aColor,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(ColorVertex),(void *)offsetof(ColorVertex,color));
	m->vertex_count = count;
}

TextureVertex *TextureVertexListMakeRoom(TextureVertexList *list, int count){
	if (list->used+count > list->total){
		if (!list->total) list->total = 1;
		while (list->used+count > list->total) list->total *= 2;
		list->elements = realloc_or_die(list->elements,list->total*sizeof(*list->elements));
	}
	list->used += count;
	return list->elements+list->used-count;
}

TextureColorVertex *TextureColorVertexListMakeRoom(TextureColorVertexList *list, int count){
	if (list->used+count > list->total){
		if (!list->total) list->total = 1;
		while (list->used+count > list->total) list->total *= 2;
		list->elements = realloc_or_die(list->elements,list->total*sizeof(*list->elements));
	}
	list->used += count;
	return list->elements+list->used-count;
}

TextureShader texture_shader = {
	"#version 330 core\n"
	"in vec3 aPosition;\n"
	"in vec2 aTexCoord;\n"
	"uniform mat4 uMVP;\n"
	"out vec2 vTexCoord;\n"
	"void main(){\n"
	"	gl_Position = uMVP * vec4(aPosition,1.0);\n"
	"	vTexCoord = aTexCoord;\n"
	"}",

	"#version 330 core\n"
	"uniform sampler2D uTex;\n"
	"in vec2 vTexCoord;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"	FragColor = texture(uTex,vTexCoord);\n"
	"}"
};

static void compile_texture_shader(){
	COMPILE_SHADER(texture_shader);
	GET_ATTRIB(texture_shader,aPosition);
	GET_ATTRIB(texture_shader,aTexCoord);
	GET_UNIFORM(texture_shader,uMVP);
	GET_UNIFORM(texture_shader,uTex);
}

TextureColorShader texture_color_shader = {
	"#version 330 core\n"
	"in vec3 aPosition;\n"
	"in vec2 aTexCoord;\n"
	"in vec4 aColor;\n"
	"uniform mat4 uMVP;\n"
	"out vec2 vTexCoord;\n"
	"out vec4 vColor;\n"
	"void main(){\n"
	"	gl_Position = uMVP * vec4(aPosition,1.0);\n"
	"	vTexCoord = aTexCoord;\n"
	"	vColor = aColor;\n"
	"}",

	"#version 330 core\n"
	"uniform sampler2D uTex;\n"
	"in vec2 vTexCoord;\n"
	"in vec4 vColor;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"	vec4 s = texture(uTex,vTexCoord);\n"
	"	FragColor = s * vColor;\n"
	"}"
};

static void compile_texture_color_shader(){
	COMPILE_SHADER(texture_color_shader);
	GET_ATTRIB(texture_color_shader,aPosition);
	GET_ATTRIB(texture_color_shader,aTexCoord);
	GET_ATTRIB(texture_color_shader,aColor);
	GET_UNIFORM(texture_color_shader,uMVP);
	GET_UNIFORM(texture_color_shader,uTex);
}

void gpu_mesh_from_texture_verts(GPUMesh *m, TextureVertex *verts, int count){
	glGenVertexArrays(1,&m->vao);
	glBindVertexArray(m->vao);
	glGenBuffers(1,&m->vbo);
	glBindBuffer(GL_ARRAY_BUFFER,m->vbo);
	glBufferData(GL_ARRAY_BUFFER,count*sizeof(*verts),verts,GL_STATIC_DRAW);
	glEnableVertexAttribArray(texture_shader.aPosition);
	glEnableVertexAttribArray(texture_shader.aTexCoord);
	glVertexAttribPointer(texture_shader.aPosition,3,GL_FLOAT,GL_FALSE,sizeof(TextureVertex),(void *)0);
	glVertexAttribPointer(texture_shader.aTexCoord,2,GL_FLOAT,GL_FALSE,sizeof(TextureVertex),(void *)offsetof(TextureVertex,texcoord));
	m->vertex_count = count;
}

void gpu_mesh_from_texture_color_verts(GPUMesh *m, TextureColorVertex *verts, int count){
	glGenVertexArrays(1,&m->vao);
	glBindVertexArray(m->vao);
	glGenBuffers(1,&m->vbo);
	glBindBuffer(GL_ARRAY_BUFFER,m->vbo);
	glBufferData(GL_ARRAY_BUFFER,count*sizeof(*verts),verts,GL_STATIC_DRAW);
	glEnableVertexAttribArray(texture_color_shader.aPosition);
	glEnableVertexAttribArray(texture_color_shader.aTexCoord);
	glEnableVertexAttribArray(texture_color_shader.aColor);
	glVertexAttribPointer(texture_color_shader.aPosition,3,GL_FLOAT,GL_FALSE,sizeof(TextureColorVertex),0);
	glVertexAttribPointer(texture_color_shader.aTexCoord,2,GL_FLOAT,GL_FALSE,sizeof(TextureColorVertex),(void *)offsetof(TextureColorVertex,texcoord));
	glVertexAttribPointer(texture_color_shader.aColor,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(TextureColorVertex),(void *)offsetof(TextureColorVertex,color));
	m->vertex_count = count;
}

void delete_gpu_mesh(GPUMesh *m){
	glDeleteBuffers(1,&m->vbo);
	glDeleteVertexArrays(1,&m->vao);
	m->vao = 0;
	m->vbo = 0;
	m->vertex_count = 0;
}

void compile_shaders(){
	compile_color_shader();
	compile_texture_shader();
	compile_texture_color_shader();
}