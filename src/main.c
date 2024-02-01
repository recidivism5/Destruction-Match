#include <glutil.h>
#include <perlin_noise.h>

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

FracturedModel banana;

GLFWwindow *gwindow;

struct {
	FracturedModelInstance *object;
	vec2 offset;
} grab;

void fmi_get_rect(FracturedModelInstance *fmi, FSRect *rect){
	rect->x = fmi->position[0];
	rect->y = fmi->position[1];
	rect->width = cellWidth;
	rect->height = cellWidth;
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
							grab.object = mi;
							vec2_sub(mi->position,mouse,grab.offset);
						}
					}
					break;
				}
				case GLFW_MOUSE_BUTTON_2:{
					insert_object(1,&banana);
					break;
				}
			}
			break;
		}
		case GLFW_RELEASE:{
			switch (button){
				case GLFW_MOUSE_BUTTON_1:{
					if (grab.object){
						grab.object = 0;
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
	gwindow = create_centered_window(1280,960,"Match Mayhem");
 
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

	load_fractured_model(&banana,"campaigns/juicebar/models/banana");

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
				} else {
					mi->position[0] = LERP(mi->position[0],boardRect.left+mi->boardPos[0]*cellWidth,6*dt);
					mi->position[1] = LERP(mi->position[1],boardRect.bottom+mi->boardPos[1]*cellWidth,6*dt);
				}
			}
		}

		if (grab.object){
			vec2_add(mouse,grab.offset,grab.object->position);
		}

		////////////// Render:
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
		glViewport((int)screen.x,(int)screen.y,(int)screen.width,(int)screen.height);

		{
			double mx,my;
			glfwGetCursorPos(gwindow,&mx,&my);
			mouse[0] = 16 * ((float)mx - screen.x) / (screen.width-1);
			mouse[1] = 9 * ((float)clientHeight-1-(float)my - screen.y) / (screen.height-1);
		}

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

					glEnableClientState(GL_VERTEX_ARRAY);
					glEnableClientState(GL_NORMAL_ARRAY);
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glEnable(GL_STENCIL_TEST);
					glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
					glStencilFunc(GL_ALWAYS,1,0xff);
					glStencilMask(0xff);
					glEnable(GL_LIGHTING);
					glVertexPointer(3,GL_FLOAT,sizeof(ModelVertex),(void *)&mi->model->vertices->position);
					glNormalPointer(GL_FLOAT,sizeof(ModelVertex),(void *)&mi->model->vertices->normal);
					glTexCoordPointer(2,GL_FLOAT,sizeof(ModelVertex),(void *)&mi->model->vertices->texcoord);
					glDrawArrays(GL_TRIANGLES,mi->model->objects[0].vertexOffsetCounts[0].offset,mi->model->objects[0].vertexOffsetCounts[0].count);
					glStencilFunc(GL_NOTEQUAL,1,0xff);
					glVertexPointer(3,GL_FLOAT,sizeof(vec3),(void *)mi->model->expandedPositions);
					glDisable(GL_TEXTURE_2D);
					glDisable(GL_LIGHTING);
					if (grab.object ? mi == grab.object : fmi_selectable(mi)){
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

			glDisable(GL_LIGHTING);
		}

		glCheckError();
 
		glfwSwapBuffers(gwindow);

		POLL:
		glfwPollEvents();
	}
 
	cleanup();
}