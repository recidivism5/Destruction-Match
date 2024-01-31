#include <glutil.h>
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

GLuint new_texture(unsigned char *pixels, int width, int height, bool interpolated){
	GLuint id;
	glGenTextures(1,&id);
	glBindTexture(GL_TEXTURE_2D,id);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,interpolated ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,interpolated ? GL_LINEAR : GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
	return id;
}

void load_texture(Texture *t, char *name, bool interpolated){
	char *path = local_path_to_absolute("res/%s",name);
	stbi_set_flip_vertically_on_load(true);
	int comp;
	unsigned char *pixels = stbi_load(path,&t->width,&t->height,&comp,4);
	if (!pixels){
		fatal_error("Failed to load texture:\n%s",path);
	}
	t->id = new_texture(pixels,t->width,t->height,interpolated);
	free(pixels);
}

void delete_texture(GLuint id){
	glDeleteTextures(1,&id);
}

void gen_font_atlas(FontAtlas *atlas, char *name, int height){
	int size;
	unsigned char *ttf = load_file(&size,"res/fonts/%s.ttf",name);
	atlas->width = 256;
	int numChars = '~'-' '+1;
	atlas->bakedChars = malloc(numChars * sizeof(*atlas->bakedChars));
	ASSERT(atlas->bakedChars);
	unsigned char *bmp;
	while (1){
		bmp = malloc(atlas->width*atlas->width);
		ASSERT(bmp);
		int r = stbtt_BakeFontBitmap(ttf,0,(float)height,bmp,atlas->width,atlas->width,' ',numChars,atlas->bakedChars);
		if (r > 0) break;
		free(bmp);
		atlas->width *= 2;
	}

	unsigned char *pixels = malloc(atlas->width * atlas->width * 4);
	ASSERT(pixels);
	for (int i = 0; i < atlas->width * atlas->width; i++){
		unsigned char *p = pixels + i*4;
		p[0] = 255;
		p[1] = 255;
		p[2] = 255;
		p[3] = bmp[i];
	}

	atlas->id = new_texture(pixels,atlas->width,atlas->width,true);

	free(pixels);
	free(ttf);
	free(bmp);
}

void load_fractured_model(FracturedModel *model, char *name){
	char *path = local_path_to_absolute("res/%s.fmf",name);
	FILE *f = fopen(path,"rb");
	if (!f){
		fatal_error("Failed to open model: %s",path);
	}
	
	ASSERT(1==fread(&model->vertexCount,sizeof(model->vertexCount),1,f));
	ASSERT(model->vertexCount < 65536);
	model->vertices = malloc(model->vertexCount * sizeof(*model->vertices));
	ASSERT(model->vertices);
	model->expandedPositions = malloc(model->vertexCount * sizeof(*model->expandedPositions));
	ASSERT(model->expandedPositions);
	ASSERT(1==fread(model->vertices,model->vertexCount * sizeof(*model->vertices),1,f));
	for (int i = 0; i < model->vertexCount; i++){
		vec3 *ev = model->expandedPositions+i;
		ModelVertex *v = model->vertices+i;
		vec3_copy(v->position,(float *)ev);
		vec3 snorm;
		vec3_scale(v->normal,0.025f,snorm);
		vec3_add((float *)ev,snorm,(float *)ev);
	}
	ASSERT(1==fread(&model->materialCount,sizeof(model->materialCount),1,f));
	ASSERT(0 < model->materialCount && model->materialCount < 256);
	model->materials = malloc(model->materialCount * sizeof(*model->materials));
	ASSERT(model->materials);
	for (Material *m = model->materials; m < model->materials + model->materialCount; m++){
		m->roughness = 0.0f;//bruh

		int compressedSize;
		ASSERT(1==fread(&compressedSize,sizeof(compressedSize),1,f));
		ASSERT(0 < compressedSize && compressedSize < 524288);
		unsigned char *compressedData = malloc(compressedSize);
		ASSERT(1==fread(compressedData,compressedSize,1,f));
		int width,height,comp;
		stbi_set_flip_vertically_on_load(true);
		unsigned char *pixels = stbi_load_from_memory(compressedData,compressedSize,&width,&height,&comp,4);
		m->textureId = new_texture(pixels,width,height,false);
		free(compressedData);
		free(pixels);
	}
	ASSERT(1==fread(&model->objectCount,sizeof(model->objectCount),1,f));
	ASSERT(model->objectCount > 0 && model->objectCount < 256);
	model->objects = malloc(model->objectCount * sizeof(*model->objects));
	ASSERT(model->objects);
	int vertexOffset = 0;
	for (FracturedObject *obj = model->objects; obj < model->objects+model->objectCount; obj++){
		ASSERT(1==fread(obj->position,sizeof(obj->position),1,f));
		obj->vertexOffsetCounts = malloc(model->materialCount * sizeof(*obj->vertexOffsetCounts));
		for (VertexOffsetCount *vic = obj->vertexOffsetCounts; vic < obj->vertexOffsetCounts + model->materialCount; vic++){
			vic->offset = vertexOffset;
			ASSERT(1==fread(&vic->count,sizeof(vic->count),1,f));
			ASSERT(vic->count <= model->vertexCount-vertexOffset);
			vertexOffset += vic->count;
		}
	}
	
	fclose(f);
}

void delete_fractured_model(FracturedModel *model){
	ASSERT(model->vertices);
	free(model->vertices);
	for (Material *m = model->materials; m < model->materials + model->materialCount; m++){
		delete_texture(m->textureId);
	}
	ASSERT(model->materials);
	free(model->materials);
	ASSERT(model->objects);
	for (FracturedObject *obj = model->objects; obj < model->objects+model->objectCount; obj++){
		ASSERT(obj->vertexOffsetCounts);
		free(obj->vertexOffsetCounts);
	}
	free(model->objects);
	memset(model,0,sizeof(*model));
}

void project_identity(){
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}

void project_ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar){
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(left,right,bottom,top,zNear,zFar);
	glMatrixMode(GL_MODELVIEW);
}

void project_perspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar){
	GLdouble top, bottom, left, right;
	top = zNear * tan((M_PI/180.0)*fovy/2);
	bottom = -top;
	right = aspect*top;
	left = -right;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(left, right, bottom, top, zNear, zFar);
	glMatrixMode(GL_MODELVIEW);
}