#include <glutil.h>

typedef struct {
	vec3 position;
	vec3 normal;
	vec2 texcoord;
} ModelVertex;

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
	FracturedModel *model;
	ivec2 boardPos;
	vec2 position;
	float yVelocity;
	float rotationRandom;
	bool locked;
} FracturedModelInstance;

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

FSRect screen;
void sub_viewport(float x, float y, float width, float height){
	glViewport(
		(int)(screen.x + screen.width * x / 16.0f),
		(int)(screen.y + screen.height * y / 9.0f),
		(int)(screen.width * width / 16.0f),
		(int)(screen.height * height / 9.0f)
	);
}
vec2 mouse;
bool mouse_in_rect(FSRect *r){
	return mouse[0] > r->x && mouse[0] < (r->x+r->width) && mouse[1] > r->y && mouse[1] < (r->y+r->height);
}

FRect boardRect;
float cellWidth;
FracturedModelInstance objects[256];
FracturedModelInstance *board[8*8];

typedef struct {
	FracturedModel *model;
	FracturedObject *object;
	vec2 position;
	vec2 velocity;
	float rotationRandom;
} Fragment;

Fragment fragments[1024];
void insert_fragment(FracturedModel *model, FracturedObject *object, vec2 position, vec2 velocity, float rotationRandom){
	for (Fragment *f = fragments; f < fragments+COUNT(fragments); f++){
		if (!f->model){
			f->model = model;
			f->object = object;
			vec2_copy(position,f->position);
			vec2_copy(velocity,f->velocity);
			f->rotationRandom = rotationRandom;
			return;
		}
	}
	ASSERT(0 && "insert_fragment overflow");
}
void explode_object(FracturedModelInstance *object){
	FracturedModel *m = object->model;
	for (int i = 1; i < m->objectCount; i++){
		FracturedObject *fo = m->objects+i;
		insert_fragment(m,fo,object->position,(vec2){(rand_int(2) ? -1 : 1) * rand_int_range(5,10),rand_int_range(5,10)},object->rotationRandom);
	}
}

void insert_object(int column, FracturedModel *model){
	for (FracturedModelInstance *fmi = objects; fmi < objects+COUNT(objects); fmi++){
		if (!fmi->model){
			fmi->model = model;
			fmi->boardPos[0] = column;
			fmi->position[0] = boardRect.left + cellWidth * column;
			fmi->position[1] = boardRect.top + 4*cellWidth;
			fmi->yVelocity = 0.0f;
			fmi->rotationRandom = (float)rand_int(400);
			return;
		}
	}
	ASSERT(0 && "insert object overflow");
}

Texture beachBackground, checker, frame;

FracturedModel 
	apple,
	banana,
	orange;

FracturedModel *models[] = {
	&apple,
	&banana,
	&orange
};

GLFWwindow *gwindow;

FracturedModelInstance *grabbedObject;

void get_rect(vec2 position, FSRect *rect){
	rect->x = position[0];
	rect->y = position[1];
	rect->width = cellWidth;
	rect->height = cellWidth;
}

void fmi_get_rect(FracturedModelInstance *fmi, FSRect *rect){
	get_rect(fmi->position,rect);
}

bool fmi_selectable(FracturedModelInstance *fmi){
	if (fmi->model && fmi->locked){
		FSRect rect;
		fmi_get_rect(fmi,&rect);
		return mouse_in_rect(&rect);
	}
	return false;
}

void cleanup(void){
	glfwDestroyWindow(gwindow);
	glfwTerminate();
}

void error_callback(int error, const char* description){
	fprintf(stderr, "Error: %s\n", description);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
	switch (action){
		case GLFW_PRESS:{
			switch (key){
				case GLFW_KEY_ESCAPE:{
					glfwSetWindowShouldClose(window, GLFW_TRUE);
					break;
				}
				case GLFW_KEY_R:{
					static int prev_width, prev_height;
					static bool fullscreen = false;
					GLFWmonitor *primary = glfwGetPrimaryMonitor();
					const GLFWvidmode *vm = glfwGetVideoMode(primary);
					if (!fullscreen){
						glfwGetFramebufferSize(window,&prev_width,&prev_height);
						glfwSetWindowAttrib(window,GLFW_FLOATING,true);
						glfwSetWindowAttrib(window,GLFW_DECORATED,false);
						glfwSetWindowMonitor(window,0,0,0,vm->width,vm->height,GLFW_DONT_CARE);
						fullscreen = true;
					} else {
						glfwSetWindowAttrib(window,GLFW_FLOATING,false);
						glfwSetWindowAttrib(window,GLFW_DECORATED,true);
						glfwSetWindowMonitor(window,0,(vm->width-prev_width)/2,(vm->height-prev_height)/2,prev_width,prev_height,GLFW_DONT_CARE);
						fullscreen = false;
					}
					break;
				}
				case GLFW_KEY_SPACE:{
					for (FracturedModelInstance *mi = objects; mi < objects+COUNT(objects); mi++){
						if (fmi_selectable(mi)){
							explode_object(mi);
							memset(mi,0,sizeof(*mi));
							for (int y = 0; y < 8; y++){
								for (int x = 0; x < 8; x++){
									if (board[y*8+x] == mi){
										board[y*8+x] = 0;
										for (int yy = y+1; yy < 8; yy++){
											if (board[yy*8+x]){
												board[yy*8+x]->locked = false;
												board[yy*8+x] = 0;
											}
										}
										goto L0;
									}
								}
							}
							L0:;
						}
					}
					break;
				}
			}
			break;
		}
		case GLFW_RELEASE:{
			switch (key){
			}
			break;
		}
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods){
	switch (action){
		case GLFW_PRESS:{
			switch (button){
				case GLFW_MOUSE_BUTTON_1:{
					for (FracturedModelInstance *mi = objects; mi < objects+COUNT(objects); mi++){
						if (fmi_selectable(mi)){
							grabbedObject = mi;
						}
					}
					break;
				}
				case GLFW_MOUSE_BUTTON_2:{
					insert_object(rand_int(8),models[rand_int(COUNT(models))]);
					break;
				}
			}
			break;
		}
		case GLFW_RELEASE:{
			switch (button){
				case GLFW_MOUSE_BUTTON_1:{
					if (grabbedObject){
						grabbedObject = 0;
					}
					break;
				}
			}
			break;
		}
	}
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos){
	
}

GLFWwindow *create_centered_window(int width, int height, char *title){
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	GLFWwindow *window = glfwCreateWindow(width,height,title,NULL,NULL);
	ASSERT(window);
	GLFWmonitor *primary = glfwGetPrimaryMonitor();
	ASSERT(primary);
	const GLFWvidmode *vm = glfwGetVideoMode(primary);
	ASSERT(vm);
	glfwSetWindowPos(window,(vm->width-width)/2,(vm->height-height)/2);
	glfwShowWindow(window);
	return window;
}

void main(void){
	glfwSetErrorCallback(error_callback);
	set_error_callback(cleanup);
 
	ASSERT(glfwInit());
 
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_SAMPLES, 4);
	gwindow = create_centered_window(1600,900,"Match Mayhem");
 
	glfwSetCursorPosCallback(gwindow, cursor_position_callback);
	glfwSetMouseButtonCallback(gwindow, mouse_button_callback);
	glfwSetKeyCallback(gwindow, key_callback);
 
	glfwMakeContextCurrent(gwindow);
	gladLoadGL();
	glfwSwapInterval(1);

	srand((unsigned int)time(0));

	//Init:
	load_texture(&beachBackground,"campaigns/juicebar/textures/background.jpg",true);
	load_texture(&checker,"textures/checker.png",false);
	load_texture(&frame,"campaigns/juicebar/textures/frame.png",true);

	load_fractured_model(&apple,"campaigns/juicebar/models/apple");
	load_fractured_model(&banana,"campaigns/juicebar/models/banana");
	load_fractured_model(&orange,"campaigns/juicebar/models/orange");

	float vpad = 9 * 0.15f;
	float hw = 0.5f * (9 - 2*vpad);
	float fhw = hw * 1.33f;
	cellWidth = hw/4;
	vec2 center = {16 * 2.0f / 3.0f,vpad + hw};
	boardRect.left = center[0]-hw;
	boardRect.right = center[0]+hw;
	boardRect.bottom = center[1]-hw;
	boardRect.top = center[1]+hw;
	vec2 fcenter = {center[0]-hw*0.014f,center[1]+hw*0.014f};

	//Loop:

	double t0 = glfwGetTime();
 
	while (!glfwWindowShouldClose(gwindow))
	{
		/////////// Update:
		double t1 = glfwGetTime();
		float dt = (float)(t1 - t0);
		t0 = t1;

		int clientWidth, clientHeight;
		glfwGetFramebufferSize(gwindow, &clientWidth, &clientHeight);
		if (!clientWidth || !clientHeight){
			goto POLL;
		}
		screen.height = (float)clientWidth * 9.0f / 16.0f;
		if (screen.height > (float)clientHeight){
			screen.width = (float)clientHeight * 16.0f / 9.0f;
			screen.height = screen.width * 9.0f / 16.0f;
			screen.x = 0.5f * (clientWidth - screen.width);
			screen.y = 0;
		} else {
			screen.width = (float)clientWidth;
			screen.x = 0;
			screen.y = 0.5f*(clientHeight - screen.height);
		}

		{
			double mx,my;
			glfwGetCursorPos(gwindow,&mx,&my);
			mouse[0] = 16 * ((float)mx - screen.x) / (screen.width-1);
			mouse[1] = 9 * ((float)clientHeight-1-(float)my - screen.y) / (screen.height-1);
		}

		for (FracturedModelInstance *mi = objects; mi < objects+COUNT(objects); mi++){
			if (mi->model){
				if (!mi->locked){
					mi->yVelocity -= 9.8f * dt;
					mi->position[1] += mi->yVelocity * dt;
					int highest = -1;
					for (int y = 7; y >= 0; y--){
						if (board[y*8+mi->boardPos[0]]){
							highest = y;
							break;
						}
					}
					ASSERT(highest < 7);
					float fhighest = boardRect.bottom+(highest+1)*cellWidth;
					if (mi->position[1] < fhighest){
						mi->position[1] = fhighest;
						mi->yVelocity = 0.0f;
						mi->locked = true;
						board[(highest+1)*8+mi->boardPos[0]] = mi;
						mi->boardPos[1] = highest+1;
					}
				} else if (mi != grabbedObject){
					vec2 target = {
						boardRect.left+mi->boardPos[0]*cellWidth,
						boardRect.bottom+mi->boardPos[1]*cellWidth
					};
					vec2_lerp(mi->position,target,7*dt,mi->position);
				}
			}
		}

		if (grabbedObject){
			vec2 target = {
				mouse[0]-cellWidth/2,
				mouse[1]-cellWidth/2
			};
			vec2_lerp(grabbedObject->position,target,20*dt,grabbedObject->position);
		}

		for (Fragment *f = fragments; f < fragments+COUNT(fragments); f++){
			if (f->model){
				f->velocity[1] -= 9.8f * dt;
				vec2 dp;
				vec2_scale(f->velocity,dt,dp);
				vec2_add(f->position,dp,f->position);
				if (f->position[0] < -1.0f || 
					f->position[0] > 16.0f ||
					f->position[1] < -1.0f ||
					f->position[1] > 9.0f){
					f->model = 0;
				}
			}
		}

		////////////// Render:
		glViewport((int)screen.x,(int)screen.y,(int)screen.width,(int)screen.height);

		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		glShadeModel(GL_SMOOTH);

		glEnable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);

		{
			glLoadIdentity();

			project_ortho(0,16,0,9,-100,1);

			glBindTexture(GL_TEXTURE_2D,beachBackground.id);
			glBegin(GL_QUADS);
			glTexCoord2f(0,0); glVertex3f(0,0,0);
			glTexCoord2f(1,0); glVertex3f(16,0,0);
			glTexCoord2f(1,1); glVertex3f(16,9,0);
			glTexCoord2f(0,1); glVertex3f(0,9,0);
			glEnd();

			glBindTexture(GL_TEXTURE_2D,checker.id);
			glBegin(GL_QUADS);
			glTexCoord2f(0,0); glVertex3f(boardRect.left,boardRect.bottom,1);
			glTexCoord2f(4,0); glVertex3f(boardRect.right,boardRect.bottom,1);
			glTexCoord2f(4,4); glVertex3f(boardRect.right,boardRect.top,1);
			glTexCoord2f(0,4); glVertex3f(boardRect.left,boardRect.top,1);
			glEnd();

			glBindTexture(GL_TEXTURE_2D,frame.id);
			glBegin(GL_QUADS);
			glTexCoord2f(0,0); glVertex3f(fcenter[0]-fhw,fcenter[1]-fhw,2);
			glTexCoord2f(1,0); glVertex3f(fcenter[0]+fhw,fcenter[1]-fhw,2);
			glTexCoord2f(1,1); glVertex3f(fcenter[0]+fhw,fcenter[1]+fhw,2);
			glTexCoord2f(0,1); glVertex3f(fcenter[0]-fhw,fcenter[1]+fhw,2);
			glEnd();
		}

		glClear(GL_DEPTH_BUFFER_BIT);

		{
			project_perspective(25.0,1.0,0.01,1000.0);

			GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
			GLfloat mat_shininess[] = { 50.0 };
			glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
			glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
			
			GLfloat light_ambient[] = { 1.5, 1.5, 1.5, 1.0 };
			GLfloat light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
			GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 };
			GLfloat light_position[] = { 1.0, 1.0, 1.0, 0.0 };
			glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
			glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
			glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
			glLightfv(GL_LIGHT0, GL_POSITION, light_position);
			glEnable(GL_LIGHT0);

			for (FracturedModelInstance *mi = objects; mi < objects+COUNT(objects); mi++){
				if (mi->model){
					FSRect rect;
					fmi_get_rect(mi,&rect);
					sub_viewport(
						rect.x,
						rect.y,
						rect.width,
						rect.height
					);

					glLoadIdentity();
					glTranslated(0,0,-2.6);
					glRotated(t0*120+mi->rotationRandom,0,1,0);

					glBindTexture(GL_TEXTURE_2D,mi->model->materials[0].textureId);

					glEnable(GL_STENCIL_TEST);
					glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
					glStencilFunc(GL_ALWAYS,1,0xff);
					glStencilMask(0xff);
					glEnable(GL_LIGHTING);

					glEnableClientState(GL_VERTEX_ARRAY);
					glEnableClientState(GL_NORMAL_ARRAY);
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glVertexPointer(3,GL_FLOAT,sizeof(ModelVertex),(void *)&mi->model->vertices->position);
					glNormalPointer(GL_FLOAT,sizeof(ModelVertex),(void *)&mi->model->vertices->normal);
					glTexCoordPointer(2,GL_FLOAT,sizeof(ModelVertex),(void *)&mi->model->vertices->texcoord);

					glDrawArrays(GL_TRIANGLES,mi->model->objects[0].vertexOffsetCounts[0].offset,mi->model->objects[0].vertexOffsetCounts[0].count);
					
					glStencilFunc(GL_NOTEQUAL,1,0xff);
					glVertexPointer(3,GL_FLOAT,sizeof(vec3),(void *)mi->model->expandedPositions);
					glDisable(GL_TEXTURE_2D);
					glDisable(GL_LIGHTING);
					if (grabbedObject ? mi == grabbedObject : fmi_selectable(mi)){
						glColor4f(1,1,1,1);
					} else {
						glColor4f(0,0,0,1);
					}
					glDrawArrays(GL_TRIANGLES,mi->model->objects[0].vertexOffsetCounts[0].offset,mi->model->objects[0].vertexOffsetCounts[0].count);
					glEnable(GL_TEXTURE_2D);
					glColor4f(1,1,1,1);
					glDisable(GL_STENCIL_TEST);

					glDisableClientState(GL_VERTEX_ARRAY);
					glDisableClientState(GL_NORMAL_ARRAY);
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				}
			}

			for (Fragment *f = fragments; f < fragments+COUNT(fragments); f++){
				if (f->model){
					FSRect rect;
					get_rect(f->position,&rect);
					sub_viewport(
						rect.x,
						rect.y,
						rect.width,
						rect.height
					);

					glLoadIdentity();
					glTranslated(0,0,-2.6);
					glRotated(t0*120+f->rotationRandom,0,1,0);

					glEnable(GL_LIGHTING);

					glEnableClientState(GL_VERTEX_ARRAY);
					glEnableClientState(GL_NORMAL_ARRAY);
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glVertexPointer(3,GL_FLOAT,sizeof(ModelVertex),(void *)&f->model->vertices->position);
					glNormalPointer(GL_FLOAT,sizeof(ModelVertex),(void *)&f->model->vertices->normal);
					glTexCoordPointer(2,GL_FLOAT,sizeof(ModelVertex),(void *)&f->model->vertices->texcoord);

					for (int i = 0; i < f->model->materialCount; i++){
						glBindTexture(GL_TEXTURE_2D,f->model->materials[i].textureId);
						glDrawArrays(GL_TRIANGLES,f->object->vertexOffsetCounts[i].offset,f->object->vertexOffsetCounts[i].count);
					}

					glDisableClientState(GL_VERTEX_ARRAY);
					glDisableClientState(GL_NORMAL_ARRAY);
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				}
			}

			glDisable(GL_LIGHTING);
		}

		glCheckError();
 
		glfwSwapBuffers(gwindow);

		POLL:
		glfwPollEvents();
	}
 
	cleanup();
}