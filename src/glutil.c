#include <glutil.h>
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

GLuint new_texture(unsigned char *pixels, int width, int height){
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

GLuint load_texture(char *name){
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

void delete_texture(GLuint id){
	glDeleteTextures(1,&id);
}

GLuint load_cubemap(char *name){
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

void load_model(Model *model, char *name){
	char *path = local_path_to_absolute("res/models/%s.bmf",name);
	FILE *f = fopen(path,"rb");
	if (!f){
		fatal_error("Failed to open model: %s",path);
	}
	ASSERT(1==fread(&model->vertexCount,sizeof(model->vertexCount),1,f));
	ASSERT(model->vertexCount < 65536);
	PhongVertex *verts = malloc(model->vertexCount * sizeof(*verts));
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
	for (PhongVertex *v = verts; v < verts + model->vertexCount; v++){
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
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(PhongVertex),0);
	glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(PhongVertex),(void *)offsetof(PhongVertex,normal));
	glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(PhongVertex),(void *)offsetof(PhongVertex,texcoord));
}

void delete_model(Model *model){
	glDeleteBuffers(1,&model->vbo);
	glDeleteVertexArrays(1,&model->vao);
	for (Material *m = model->materials; m < model->materials + model->materialCount; m++){
		delete_texture(m->textureId);
	}
	ASSERT(model->materials);
	free(model->materials);
	memset(model,0,sizeof(*model));
}

void shader_set_mat4(GLuint id, char *name, float *mat, bool transpose){
	glUniformMatrix4fv(glGetUniformLocation(id,name),1,transpose,mat);
}

void shader_set_int(GLuint id, char *name, int val){
	glUniform1i(glGetUniformLocation(id,name),val);
}

void shader_set_float(GLuint id, char *name, float val){
	glUniform1f(glGetUniformLocation(id,name),val);
}

void shader_set_vec3(GLuint id, char *name, vec3 v){
	glUniform3fv(glGetUniformLocation(id,name),1,v);
}