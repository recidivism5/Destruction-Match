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

static GLuint new_texture(unsigned char *pixels, int width, int height){
	GLuint id;
	glGenTextures(1,&id);
	glBindTexture(GL_TEXTURE_2D,id);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
	return id;
}

static GLuint load_texture(char *name){
	char *path = local_path_to_absolute("res/textures/%s.png",name);
	stbi_set_flip_vertically_on_load(true);
	int width, height, comp;
	unsigned char *pixels = stbi_load(path,&width,&height,&comp,4);
	if (!pixels){
		fatal_error("Failed to load texture:\n%s",path);
	}
	GLuint id = new_texture(pixels,width,height);
	free(pixels);
	return id;
}

static delete_texture(GLuint id){
	glDeleteTextures(1,&id);
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

static void load_model(Model *model, char *name){
	char *path = local_path_to_absolute("res/models/%s.bmf",name);
	FILE *f = fopen(path,"rb");
	if (!f){
		fatal_error("Failed to open model: %s",path);
	}
	ASSERT(1==fread(&model->vertexCount,sizeof(model->vertexCount),1,f));
	ASSERT(model->vertexCount < 65536);
	BlinnPhongVertex *verts = malloc(model->vertexCount * sizeof(*verts));
	ASSERT(1==fread(verts,model->vertexCount * sizeof(*verts),1,f));
	ASSERT(1==fread(&model->materialCount,sizeof(model->materialCount),1,f));
	ASSERT(0 < model->materialCount && model->materialCount < 256);
	model->materials = malloc(model->materialCount * sizeof(*model->materials));
	int vertexOffset = 0;
	for (Material *m = model->materials; m < model->materials + model->materialCount; m++){
		m->vertexOffset = vertexOffset;
		ASSERT(1==fread(&m->vertexCount,sizeof(m->vertexCount),1,f));
		ASSERT(m->vertexCount <= model->vertexCount-vertexOffset);
		vertexOffset += m->vertexCount;
		ASSERT(1==fread(&m->glass,sizeof(m->glass),1,f));
		ASSERT(0 <= m->glass && m->glass <= 1);
		ASSERT(1==fread(&m->roughness,sizeof(m->roughness),1,f));
		ASSERT(0.0f <= m->roughness && m->roughness <= 1.0f);
		int compressedSize;
		ASSERT(1==fread(&compressedSize,sizeof(compressedSize),1,f));
		ASSERT(0 < compressedSize && compressedSize < 524288);
		unsigned char *compressedData = malloc(compressedSize);
		ASSERT(1==fread(compressedData,compressedSize,1,f));
		int width,height,comp;
		unsigned char *pixels = stbi_load_from_memory(compressedData,compressedSize,&width,&height,&comp,4);
		m->textureId = new_texture(pixels,width,height);
		free(compressedData);
		free(pixels);
	}
	fclose(f);
	for (int i = 0; i < 3; i++){
		model->boundingBox.max[i] = -INFINITY;
		model->boundingBox.min[i] = INFINITY;
	}
	for (BlinnPhongVertex *v = verts; v < verts + model->vertexCount; v++){
		for (int i = 0; i < 3; i++){
			if (v->position[i] < model->boundingBox.min[i]){
				model->boundingBox.min[i] = v->position[i];
			} else if (v->position[i] > model->boundingBox.max[i]){
				model->boundingBox.max[i] = v->position[i];
			}
		}
	}
	glGenVertexArrays(1,&model->vao);
	glBindVertexArray(model->vao);
	glGenBuffers(1,&model->vbo);
	glBindBuffer(GL_ARRAY_BUFFER,model->vbo);
	glBufferData(GL_ARRAY_BUFFER,model->vertexCount*sizeof(*verts),verts,GL_STATIC_DRAW);
	free(verts);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(BlinnPhongVertex),0);
	glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(BlinnPhongVertex),(void *)offsetof(BlinnPhongVertex,normal));
	glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(BlinnPhongVertex),(void *)offsetof(BlinnPhongVertex,texcoord));
}

static void delete_model(Model *model){
	glDeleteBuffers(1,&model->vbo);
	glDeleteVertexArrays(1,&model->vao);
	for (Material *m = model->materials; m < model->materials + model->materialCount; m++){
		delete_texture(m->textureId);
	}
	ASSERT(model->materials);
	free(model->materials);
	memset(model,0,sizeof(*model));
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

static struct {
	GLuint id;
} blinnPhongShader;

static struct {
	GLuint id, position, normal, albedoSpec, depth;
	int width, height;
} deferredFbo;

static void init_deferred_fbo(int width, int height){
	if (deferredFbo.id){
		glDeleteFramebuffers(1,&deferredFbo.id);
		delete_texture(deferredFbo.position);
		delete_texture(deferredFbo.normal);
		delete_texture(deferredFbo.albedoSpec);
		memset(&deferredFbo,0,sizeof(deferredFbo));
	}
	glGenFramebuffers(1,&deferredFbo.id);
	glBindFramebuffer(GL_FRAMEBUFFER,deferredFbo.id);

	glGenTextures(1,&deferredFbo.position);
	glBindTexture(GL_TEXTURE_2D,deferredFbo.position);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_FLOAT,0);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,deferredFbo.position,0);

	glGenTextures(1,&deferredFbo.normal);
	glBindTexture(GL_TEXTURE_2D,deferredFbo.normal);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_FLOAT,0);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,deferredFbo.normal,0);
	
	glGenTextures(1,&deferredFbo.albedoSpec);
	glBindTexture(GL_TEXTURE_2D,deferredFbo.albedoSpec);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D,deferredFbo.albedoSpec,0);

	unsigned int attachments[3] = {GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3,attachments);
    
    glGenRenderbuffers(1,&deferredFbo.depth);
    glBindRenderbuffer(GL_RENDERBUFFER,deferredFbo.depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT,width,height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,deferredFbo.depth);
	
    ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

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

	//blinnPhongShader.id = load_shader("blinn_phong");
}

void render(GLFWwindow *window, double dt){
	int width,height;
	glfwGetFramebufferSize(window,&width,&height);
	if (width != deferredFbo.width || height != deferredFbo.height){
		init_deferred_fbo(width,height);
	}

	glBindFramebuffer(GL_FRAMEBUFFER,deferredFbo.id);
	glClearColor(1.0f,0.0f,0.0f,1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER,0);
}