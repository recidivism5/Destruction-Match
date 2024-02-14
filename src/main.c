#include <glutil.h>
#include <alutil.h>
#include <perlin_noise.h>

////////////TYPES:

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

typedef enum {
	UNPLACED,
	FALLING,
	SETTLED,
	ANIMATING
} ObjectState;

typedef struct FracturedModelInstance{
	FracturedModel *model;
	vec2 position;
	float yVelocity;
	float rotationRandom;
	ObjectState state;
} FracturedModelInstance;

typedef struct {
	FracturedModel *model;
	FracturedObject *object;
	vec2 position;
	vec2 velocity;
	float rotationRandom;
} Fragment;

typedef struct {
	ivec2 position;
	int direction;
} EmptyPair;

typedef struct {
	bool active;
	ivec2 src, dst;
	float t;
} Move;

////////////////GLOBALS:

#define GRAVITY (-2.0f*9.8f)

GLFWwindow *gwindow;
ALCdevice *alcDevice;
ALCcontext *alcContext;

ALuint bruh;

FontAtlas font;

FSRect screen;

vec2 mouse;

FRect boardRect;
float cellWidth;
FracturedModelInstance board[8][8];//by column,row

Fragment fragments[1024];

Texture beachBackground, checker, frame, bubbleTexture, tubeFrontTexture, wideButtonDark, wideButtonLight;

FracturedModel models[6];

FracturedModelInstance *grabbedObject;

EmptyPair pairs[128];
int pairCount;

FracturedModelInstance *marked[64*2];
int markedCount;

Move move;

vec2 bubbles[32];

double t0,t1;
float dt;

float timeRemaining;
int targetItems;
int totalItems;

float fontHeight;
float fontScale;

#define FONT_HEIGHT_PIX 400

enum {
	ON_MENU,
	PLAYING,
} gameState = ON_MENU, targetGameState;

float uiY;

bool justClicked;

enum {
	FADE_NONE,
	FADE_OUT,
	FADE_IN
} fadeState;
float fadeAlpha;

float slideX;

bool fadeZoneTitle;
float zoneTitleAlpha;

////////////////END GLOBALS

void draw_circle(float x, float y, float z, float radius, int vertices){
	glBegin(GL_TRIANGLE_FAN);
	float c = 2.0f*(float)M_PI/vertices;
	float t = 0.0f;
	glVertex3f(x,y,z);
	for (int i = 0; i < vertices; i++){
		glVertex3f(x+radius*cosf(t),y+radius*sinf(t),z);
		t += c;
	}
	glVertex3f(x+radius,y,z);
	glEnd();
}

void init_bubbles(){
	for (int i = 0; i < COUNT(bubbles); i++){
		bubbles[i][0] = rand_float(0.0f,16.0f);
		bubbles[i][1] = rand_float(0.0f,9.0f);
	}
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

void sub_viewport(float x, float y, float width, float height){
	glViewport(
		(int)(screen.x + screen.width * x / 16.0f),
		(int)(screen.y + screen.height * y / 9.0f),
		(int)(screen.width * width / 16.0f),
		(int)(screen.height * height / 9.0f)
	);
}

bool mouse_in_rect(FSRect *r){
	return mouse[0] > r->x && mouse[0] < (r->x+r->width) && mouse[1] > r->y && mouse[1] < (r->y+r->height);
}

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
	if (object->state == SETTLED){
		for (int i = 1; i < object->model->objectCount; i++){
			FracturedObject *fo = object->model->objects+i;
			insert_fragment(
				object->model,
				fo,
				object->position,
				(vec2){
					rand_float(-10,10),
					rand_float(5,10)
				},
				object->rotationRandom
			);
		}
		totalItems++;
		if (totalItems > targetItems){
			totalItems = targetItems;
		}
		timeRemaining += 0.005f;
		if (timeRemaining > 1.0f){
			timeRemaining = 1.0f;
		}
		object->state = UNPLACED;
	}
}

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
	if (fmi->state == SETTLED){
		FSRect rect;
		fmi_get_rect(fmi,&rect);
		return mouse_in_rect(&rect);
	}
	return false;
}

bool get_mouse_cell(ivec2 cell){
	cell[0] = (int)floorf((mouse[0] - boardRect.left) / cellWidth);
	cell[1] = (int)floorf((mouse[1] - boardRect.bottom) / cellWidth);
	return (cell[0] >= 0 && cell[0] <= 7 && cell[1] >= 0 && cell[1] <= 7);
}

void append_pair(int x, int y, int direction){
	ASSERT(pairCount < COUNT(pairs));
	pairs[pairCount].position[0] = x;
	pairs[pairCount].position[1] = y;
	pairs[pairCount].direction = direction;
	pairCount++;
}

void mark(int x, int y){
	ASSERT(markedCount < COUNT(marked));
	marked[markedCount++] = &board[x][y];
}

bool lock_match(int x, int y){
	//locks 1 minimal match including (x,y)
	//returns false if no match found
	int count = 0;
	for (int xx = MAX(0,x-2); xx <= MIN(7,x+2); xx++){
		if (board[xx][y].state == SETTLED && board[xx][y].model == board[x][y].model){
			count++;
			if (count == 3){
				board[xx][y].state = ANIMATING;
				board[xx-1][y].state = ANIMATING;
				board[xx-2][y].state = ANIMATING;
				return true;
			}
		} else {
			count = 0;
		}
	}
	count = 0;
	for (int yy = MAX(0,y-2); yy <= MIN(7,y+2); yy++){
		if (board[x][yy].state == SETTLED && board[x][yy].model == board[x][y].model){
			count++;
			if (count == 3){
				board[x][yy].state = ANIMATING;
				board[x][yy-1].state = ANIMATING;
				board[x][yy-1].state = ANIMATING;
				return true;
			}
		} else {
			count = 0;
		}
	}
	return false;
}

void draw_meter(float x, float r, float g, float b, float level){
	float meterHw = 1.0f/3.0f;
	float meterLeft = x-meterHw;
	float samples[8];
	for (int i = 0; i < COUNT(samples); i++){
		samples[i] = 0.125f*perlin_noise_1d((float)t0/2.0f+(float)i/8.0f);
	}
	float barWidth = meterHw * 2.0f / (COUNT(samples)-1);

	float top = boardRect.bottom + level*(boardRect.top-boardRect.bottom);

	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	for (int i = 0; i < COUNT(samples)-1; i++){
		glColor4f(0.75f*r,0.75f*g,0.75f*b,0.95f); glVertex3f(meterLeft+i*barWidth,boardRect.bottom,3);
		glColor4f(0.75f*r,0.75f*g,0.75f*b,0.95f); glVertex3f(meterLeft+i*barWidth+barWidth,boardRect.bottom,3);
		glColor4f(r,g,b,0.95f); glVertex3f(meterLeft+i*barWidth+barWidth,top+samples[i+1],3);
		glColor4f(r,g,b,0.95f); glVertex3f(meterLeft+i*barWidth,top+samples[i],3);
	}
	glEnd();
	glEnable(GL_TEXTURE_2D);

	glColor4f(1.0f,1.0f,1.0f,1.0f);

	float tubeHw = 3.4f*meterHw/2;
	glBindTexture(GL_TEXTURE_2D,tubeFrontTexture.id);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f,0.0f); glVertex3f(x-tubeHw,boardRect.bottom-0.35f,4);
	glTexCoord2f(1.0f,0.0f); glVertex3f(x+tubeHw,boardRect.bottom-0.35f,4);
	glTexCoord2f(1.0f,1.0f); glVertex3f(x+tubeHw,boardRect.top,4);
	glTexCoord2f(0.0f,1.0f); glVertex3f(x-tubeHw,boardRect.top,4);
	glEnd();
}

void set_font_height(float height){
	fontHeight = height;
	fontScale = fontHeight / FONT_HEIGHT_PIX;
}

void draw_text(float x, float y, float z, char *text){
	glBegin(GL_QUADS);
	while (*text){
		stbtt_bakedchar *bc = font.bakedChars+(*text)-' ';
		FRect r = {
			.left = x + fontScale*bc->xoff,
			.top = y - fontScale*bc->yoff
		};
		r.right = r.left + fontScale*(float)(bc->x1-bc->x0);
		r.bottom = r.top - fontScale*(float)(bc->y1-bc->y0);
		glTexCoord2f((float)bc->x0/font.width,(float)bc->y1/font.width); glVertex3f(r.left,r.bottom,z);
		glTexCoord2f((float)bc->x1/font.width,(float)bc->y1/font.width); glVertex3f(r.right,r.bottom,z);
		glTexCoord2f((float)bc->x1/font.width,(float)bc->y0/font.width); glVertex3f(r.right,r.top,z);
		glTexCoord2f((float)bc->x0/font.width,(float)bc->y0/font.width); glVertex3f(r.left,r.top,z);
		x += fontScale*bc->xadvance;
		text++;
	}
	glEnd();
}

void draw_text_centered(float x, float y, float z, char *text){
	float width = 0.0f;
	size_t len = strlen(text);
	stbtt_bakedchar *bc;
	for (size_t i = 0; i < len-1; i++){
		bc = font.bakedChars+text[i]-' ';
		width += fontScale*bc->xadvance;
	}
	bc = font.bakedChars+text[len-1]-' ';
	width += fontScale*(bc->x1-bc->x0);
	draw_text(x-width/2,y-fontHeight/4,z,text);
}

void draw_text_shadow(float x, float y, float z, char *text){
	glColor3f(0,0,0);
	float off = fontScale * 8.0f;
	draw_text(x+off,y-off,z,text);
	glColor3f(1,1,1);
	draw_text(x,y,z+1,text);
}

void draw_text_shadow_centered(float x, float y, float z, char *text){
	glColor3f(0,0,0);
	float off = fontScale * 8.0f;
	draw_text_centered(x+off,y-off,z,text);
	glColor3f(1,1,1);
	draw_text_centered(x,y,z+1,text);
}

void draw_text_shadow_centered_alpha(float x, float y, float z, float alpha, char *text){
	glColor4f(0,0,0,alpha);
	float off = fontScale * 8.0f;
	draw_text_centered(x+off,y-off,z,text);
	glColor4f(1,1,1,alpha);
	draw_text_centered(x,y,z+1,text);
}

void draw_button(float x, float y, float z, char *text){
	glBindTexture(GL_TEXTURE_2D,wideButtonDark.id);
	float aspect = 293.0f/75.0f;
	float width = fontHeight * aspect;
	x -= width/2;
	y -= fontHeight/2;
	glColor3f(1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0); glVertex3f(x,y,z);
	glTexCoord2f(1,0); glVertex3f(x+width,y,z);
	glTexCoord2f(1,1); glVertex3f(x+width,y+fontHeight,z);
	glTexCoord2f(0,1); glVertex3f(x,y+fontHeight,z);
	glEnd();
	glBindTexture(GL_TEXTURE_2D,font.id);
	draw_text_shadow_centered(x+width/2,y+fontHeight/2,z+1,text);
}

void ui_begin(float y){
	uiY = y;
	set_font_height(1.0f);
	glEnable(GL_TEXTURE_2D);
}

void ui_end(){
	glDisable(GL_TEXTURE_2D);
}

bool ui_button(char *text){
	float aspect = 293.0f/75.0f;
	FSRect rect = {
		.width = fontHeight * aspect,
		.height = fontHeight
	};
	rect.x = 8.0f - rect.width/2;
	rect.y = uiY - fontHeight/2;
	bool hovered = mouse_in_rect(&rect);
	glBindTexture(GL_TEXTURE_2D,hovered ? wideButtonLight.id : wideButtonDark.id);
	glColor3f(1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0); glVertex3f(rect.x,rect.y,2);
	glTexCoord2f(1,0); glVertex3f(rect.x+rect.width,rect.y,2);
	glTexCoord2f(1,1); glVertex3f(rect.x+rect.width,rect.y+fontHeight,2);
	glTexCoord2f(0,1); glVertex3f(rect.x,rect.y+fontHeight,2);
	glEnd();
	glBindTexture(GL_TEXTURE_2D,font.id);
	draw_text_shadow_centered(rect.x+rect.width/2,rect.y+fontHeight/2,3,text);
	uiY -= 1.5f;
	return hovered && justClicked;
}

void cleanup(void){
	glfwDestroyWindow(gwindow);
	glfwTerminate();
	delete_sound_sources();
	alDeleteBuffers(1,&bruh);
	alcMakeContextCurrent(0);
	alcDestroyContext(alcContext);
	alcCloseDevice(alcDevice);
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
					for (int x = 0; x < 8; x++){
						for (int y = 0; y < 8; y++){
							FracturedModelInstance *mi = &board[x][y];
							if (fmi_selectable(mi)){
								grabbedObject = mi;
							}
						}
					}
					break;
				}
			}
			break;
		}
		case GLFW_RELEASE:{
			switch (button){
				case GLFW_MOUSE_BUTTON_1:{
					justClicked = true;
					if (grabbedObject){
						ivec2 src = {
							(int)((grabbedObject - &board[0][0]) / 8),
							(int)((grabbedObject - &board[0][0]) % 8)
						};
						ivec2 dst;
						if (
							get_mouse_cell(dst) &&
							ivec2_manhattan(src,dst) == 1
						){
							FracturedModelInstance
								*si = &board[src[0]][src[1]],
								*di = &board[dst[0]][dst[1]];
							if (si->state == SETTLED && di->state == SETTLED){
								FracturedModel *temp;
								SWAP(temp,si->model,di->model);
								if (lock_match(src[0],src[1]) || lock_match(dst[0],dst[1])){
									si->state = ANIMATING;
									di->state = ANIMATING;
									if (!move.active){
										move.active = true;
										move.t = 0.0f;
										ivec2_copy(src,move.src);
										ivec2_copy(dst,move.dst);
									}
								}
								SWAP(temp,si->model,di->model);
							}
						}
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
	gwindow = create_centered_window(640,480,"Match Mayhem");
 
	glfwSetCursorPosCallback(gwindow, cursor_position_callback);
	glfwSetMouseButtonCallback(gwindow, mouse_button_callback);
	glfwSetKeyCallback(gwindow, key_callback);
 
	glfwMakeContextCurrent(gwindow);
	gladLoadGL();
	glfwSwapInterval(1);

	alcDevice = alcOpenDevice(0);
	ASSERT(alcDevice);
	alcContext = alcCreateContext(alcDevice,0);
	ASSERT(alcContext);
	ASSERT(alcMakeContextCurrent(alcContext));
	init_sound_sources();

	srand((unsigned int)time(0));

	//Init:
	gen_font_atlas(&font,"Nunito-Regular",FONT_HEIGHT_PIX);

	load_texture(&beachBackground,"campaigns/juicebar/textures/background.jpg",true);
	load_texture(&checker,"textures/checker.png",false);
	load_texture(&frame,"campaigns/juicebar/textures/frame.png",true);
	load_texture(&bubbleTexture,"textures/bubble.png",true);
	load_texture(&tubeFrontTexture,"textures/tube_front10.png",true);
	load_texture(&wideButtonDark,"textures/wide_button_dark.png",true);
	load_texture(&wideButtonLight,"textures/wide_button_light.png",true);

	load_fractured_model(models+0,"campaigns/juicebar/models/apple");
	load_fractured_model(models+1,"campaigns/juicebar/models/banana");
	load_fractured_model(models+2,"campaigns/juicebar/models/orange");
	load_fractured_model(models+3,"campaigns/juicebar/models/pineapple");
	load_fractured_model(models+4,"campaigns/juicebar/models/watermelon");
	load_fractured_model(models+5,"campaigns/juicebar/models/grapes");

	bruh = load_sound("bruh");

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

	init_bubbles();

	timeRemaining = 1.0f;
	targetItems = 100;
	totalItems = 0;

	//Loop:

	t0 = glfwGetTime();
 
	while (!glfwWindowShouldClose(gwindow)){
		/////////// Update:
		t1 = glfwGetTime();
		dt = (float)(t1 - t0);
		t0 = t1;

		timeRemaining -= dt * 1.0f / 60;

		//dt *= 0.125f;

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

		glViewport((int)screen.x,(int)screen.y,(int)screen.width,(int)screen.height);
		project_ortho(0,16,0,9,-100,1);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glShadeModel(GL_SMOOTH);
		glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);

		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		slideX -= 7.0f*dt;
		if (slideX <= 0.0f){
			slideX = 0.0f;
			fadeZoneTitle = true;
		}
		if (fadeZoneTitle){
			zoneTitleAlpha -= dt;
			if (zoneTitleAlpha < 0.0f){
				zoneTitleAlpha = 0.0f;
			}
		}

		if (gameState == ON_MENU){
			glBegin(GL_QUADS);
			glColor3f(1,0,0); glVertex3f(0,0,0);
			glColor3f(0,1,0); glVertex3f(16,0,0);
			glColor3f(0,0,1); glVertex3f(16,9,0);
			glColor3f(1,1,0); glVertex3f(0,9,0);
			glEnd();

			glColor3f(0,0,0);
			static float offset = -1.5f;
			offset += dt;
			if (offset >= 0.0f){
				offset = -1.5f;
			}
			for (int j = 0; j < 8; j++){
				for (int i = 0; i < 8; i++){
					draw_circle(offset+(j%2)*1.5f+i*3.0f,offset+j*1.5f,1,0.25f,32);
				}
			}

			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D,font.id);
			set_font_height(2.0f);
			draw_text_shadow(3,6,1,"Match Mayhem");
			glDisable(GL_TEXTURE_2D);

			ui_begin(4.5f);
			if (targetGameState == PLAYING){
				justClicked = false;
			}
			if (ui_button("Play")){
				targetGameState = PLAYING;
				fadeState = FADE_OUT;
				slideX = 16.0f;
				zoneTitleAlpha = 1.0f;
				fadeZoneTitle = false;
			}
			ui_button("Settings");
			if (ui_button("Quit")){
				glfwSetWindowShouldClose(gwindow,GLFW_TRUE);
			}
			ui_end();
		} else if (gameState == PLAYING){
			//explode matches:
			//ignore everything except SETTLED objects, just mark and blow up the old fashioned way
			//moves do local checks and mark the 2 nearest matching objects as ANIMATING along with the moved object
			/*
			this needs to mark objects for explosion and then explode them.
			otherwise we miss multi-matches
			*/
			markedCount = 0;
			FracturedModel *last;
			int same;
			for (int y = 0; y < 8; y++){
				same = 0;
				last = 0;
				for (int x = 0; x < 8; x++){
					if (board[x][y].state != SETTLED){
						last = 0;
						same = 0;
						continue;
					}
					FracturedModel *m = board[x][y].model;
					if (last != m){
						same = 1;
						last = m;
					} else {
						same++;
					}
					if (last){
						if (same == 3){
							for (int xx = x-2; xx <= x; xx++){
								mark(xx,y);
							}
						} else if (same > 3){
							mark(x,y);
						}
					}
				}
			}
			for (int x = 0; x < 8; x++){
				same = 0;
				last = 0;
				for (int y = 0; y < 8; y++){
					if (board[x][y].state != SETTLED){
						last = 0;
						same = 0;
						continue;
					}
					FracturedModel *m = board[x][y].model;
					if (last != m){
						same = 1;
						last = m;
					} else {
						same++;
					}
					if (last){
						if (same == 3){
							for (int yy = y-2; yy <= y; yy++){
								mark(x,yy);
							}
						} else if (same > 3){
							mark(x,y);
						}
					}
				}
			}
			if (markedCount){
				play_sound(bruh);
			}
			for (int i = 0; i < markedCount; i++){
				explode_object(marked[i]);
			}
			//detect UNPLACED:
			bool unplacedFound = false;
			for (int x = 0; x < 8; x++){
				for (int y = 0; y < 8; y++){
					if (board[x][y].state == UNPLACED){
						unplacedFound = true;
						goto L1;
					}
				}
			}
			L1:;
			if (unplacedFound){
				//propagate UNPLACED up:
				for (int x = 0; x < 8; x++){
					FracturedModelInstance *mi = &board[x][0];
					while (mi->state != UNPLACED){
						mi++;
						if (mi - board[x] >= 8){
							goto L0;
						}
					}
					for (int y = (int)(mi-(&board[x][0]))+1; y < 8; y++){
						if (board[x][y].state != UNPLACED){
							*mi = board[x][y];
							mi->state = FALLING;
							mi++;
							board[x][y].state = UNPLACED;
						}
					}
					L0:;
				}
				//randomize UNPLACED:
				for (int x = 0; x < 8; x++){
					for (int y = 0; y < 8; y++){
						if (board[x][y].state == UNPLACED){
							board[x][y].model = models+rand_int(COUNT(models));
						}
					}
				}
				//remove matches:
				bool found;
				do {
					//need to ensure at least 1 potential match here
					//find pairs of UNPLACED:
					pairCount = 0;
					for (int y = 0; y < 8; y++){
						for (int x = 0; x < 7; x++){
							if (
								board[x][y].state == UNPLACED &&
								board[x+1][y].state == UNPLACED
							){
								append_pair(x,y,0);
							}
						}
					}
					for (int x = 0; x < 8; x++){
						for (int y = 0; y < 7; y++){
							if (
								board[x][y].state == UNPLACED &&
								board[x][y+1].state == UNPLACED
							){
								append_pair(x,y,1);
							}
						}
					}
					//pick random pair and make a potential match with it:
					//TODO: need to only do this if there isn't a potential match on the board already
					//TODO: this only checks inline matches
					for (int x = 0; x < 8; x++){
						for (int y = 0; y <= 4; y++){
							if (
								(board[x][y].model == board[x][y+1].model && board[x][y].model == board[x][y+3].model) ||
								(board[x][y].model == board[x][y+2].model && board[x][y].model == board[x][y+3].model)
							){
								goto L2;
							}
						}
					}
					for (int y = 0; y < 8; y++){
						for (int x = 0; x <= 4; x++){
							if (
								(board[x][y].model == board[x+1][y].model && board[x][y].model == board[x+3][y].model) ||
								(board[x][y].model == board[x+2][y].model && board[x][y].model == board[x+3][y].model)
							){
								goto L2;
							}
						}
					}
					{
						//TODO: this only makes inline matches, it should make other types of matches too.
						ASSERT(pairCount > 0);
						EmptyPair *p = pairs + rand_int(pairCount);
						int x = p->position[0];
						int y = p->position[1];
						FracturedModel *m;
						if (p->direction == 0){
							//horizontal
							if (p->position[0] < 2){
								m = board[x+3][y].model;
							} else if (p->position[0] < 5){
								if (rand_int(2)){
									m = board[x+3][y].model;
								} else {
									m = board[x-2][y].model;
								}
							} else {
								m = board[x-2][y].model;
							}
							board[x+1][y].model = m;
						} else {
							//vertical
							if (p->position[1] < 2){
								m = board[x][y+3].model;
							} else if (p->position[1] < 5){
								if (rand_int(2)){
									m = board[x][y+3].model;
								} else {
									m = board[x][y-2].model;
								}
							} else {
								m = board[x][y-2].model;
							}
							board[x][y+1].model = m;
						}
						board[x][y].model = m;
					}
					L2:;
					//remove matches:
					found = false;
					for (int y = 0; y < 8; y++){
						last = 0;
						same = 0;
						for (int x = 0; x < 8; x++){
							if (board[x][y].state != UNPLACED){
								last = 0;
								same = 0;
								continue;
							}
							FracturedModel *m = board[x][y].model;
							if (last != m){
								same = 1;
								last = m;
							} else {
								same++;
							}
							if (same == 3){
								int id = (int)(m-models);
								int r = rand_int(COUNT(models)-1);
								board[x][y].model = r >= id ? models+r+1 : models+r;
								found = true;
							}
						}
					}
					for (int x = 0; x < 8; x++){
						last = 0;
						same = 0;
						for (int y = 0; y < 8; y++){
							if (board[x][y].state != UNPLACED){
								last = 0;
								same = 0;
								continue;
							}
							FracturedModel *m = board[x][y].model;
							if (last != m){
								same = 1;
								last = m;
							} else {
								same++;
							}
							if (same == 3){
								int id = (int)(m-models);
								int r = rand_int(COUNT(models)-1);
								board[x][y].model = r >= id ? models+r+1 : models+r;
								found = true;
							}
						}
					}
				} while (found);
			}
			//update:
			float unplacedInc = cellWidth*1.2f;
			for (int x = 0; x < 8; x++){
				float unplacedY = 9.1f;
				for (int y = 0; y < 8; y++){
					float top = board[x][y].position[1]+unplacedInc;
					if (top > unplacedY){
						unplacedY = top;
					}
				}
				for (int y = 0; y < 8; y++){
					FracturedModelInstance *mi = &board[x][y];
					if (mi->state == UNPLACED){
						mi->position[0] = boardRect.left + x*cellWidth;
						mi->position[1] = unplacedY;
						mi->rotationRandom = (float)rand_int(360);
						mi->yVelocity = 0.0f;
						mi->state = FALLING;
						unplacedY += unplacedInc;
					} else if (mi->state == FALLING){
						mi->yVelocity += GRAVITY * dt;
						if (mi->yVelocity < -9.8f){
							mi->yVelocity = -9.8f;
						}
						mi->position[1] += mi->yVelocity * dt;
						if (y){
							float top = board[x][y-1].position[1] + cellWidth - 0.001f;
							if (mi->position[1] < top){
								mi->position[1] = top;
								mi->yVelocity = 0.0f;
							}
						}
						float rest = boardRect.bottom+y*cellWidth;
						if (mi->position[1] < rest){
							mi->position[1] = rest;
							mi->yVelocity = 0.0f;
							mi->state = SETTLED;
						}
					}
				}
			}
			//animate move:
			if (move.active){
				FracturedModelInstance
					*si = &board[move.src[0]][move.src[1]],
					*di = &board[move.dst[0]][move.dst[1]];
				vec2 spos = {
					boardRect.left + move.src[0]*cellWidth,
					boardRect.bottom + move.src[1]*cellWidth
				};
				vec2 dpos = {
					boardRect.left + move.dst[0]*cellWidth,
					boardRect.bottom + move.dst[1]*cellWidth
				};
				move.t += 5*dt;
				if (move.t >= 1.0f){
					move.active = false;
					for (int x = 0; x < 8; x++){
						for (int y = 0; y < 8; y++){
							if (board[x][y].state == ANIMATING){
								board[x][y].state = SETTLED;
							}
						}
					}
					FracturedModelInstance temp;
					SWAP(temp,*si,*di);
					vec2_copy(spos,si->position);
					vec2_copy(dpos,di->position);
				} else {
					vec2_lerp(spos,dpos,move.t,si->position);
					vec2_lerp(dpos,spos,move.t,di->position);
				}
			}
			//animate fragments:
			for (Fragment *f = fragments; f < fragments+COUNT(fragments); f++){
				if (f->model){
					f->velocity[1] += GRAVITY * dt;
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
			glLoadIdentity();
			glTranslatef(slideX,0,0);

			glEnable(GL_TEXTURE_2D);
			glColor4f(1,1,1,1);

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

			draw_meter(4.0f + 0.75f,1.0f,0.0f,0.0f,timeRemaining);
			draw_meter(4.0f - 0.75f,0.0f,1.0f,0.0f,(float)totalItems/targetItems);

			glClear(GL_DEPTH_BUFFER_BIT);

			GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
			GLfloat mat_shininess[] = { 50.0 };
			glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
			glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
			
			GLfloat light_ambient[] = { 1.5, 1.5, 1.5, 1.0 };
			GLfloat light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
			GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 };
			GLfloat light_position[] = { 1.0, 1.0, 3.0, 0.0 };
			glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
			glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
			glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
			glLightfv(GL_LIGHT0, GL_POSITION, light_position);
			glEnable(GL_LIGHT0);

			for (int x = 0; x < 8; x++){
				for (int y = 0; y < 8; y++){
					FracturedModelInstance *mi = &board[x][y];
					if (mi->model){
						FSRect rect;
						fmi_get_rect(mi,&rect);

						glLoadIdentity();
						glTranslatef(slideX,0,0);
						glTranslated(rect.x+0.5f*rect.width,rect.y+0.5f*rect.height,10);
						glRotated(t0*120+mi->rotationRandom,0,1,0);
						glScaled(0.75f,0.75f,0.75f);

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

						for (int i = 0; i < mi->model->materialCount; i++){
							glBindTexture(GL_TEXTURE_2D,mi->model->materials[i].textureId);
							glDrawArrays(GL_TRIANGLES,mi->model->objects[0].vertexOffsetCounts[i].offset,mi->model->objects[0].vertexOffsetCounts[i].count);
						}

						glStencilFunc(GL_NOTEQUAL,1,0xff);
						glVertexPointer(3,GL_FLOAT,sizeof(vec3),(void *)mi->model->expandedPositions);
						glDisable(GL_TEXTURE_2D);
						glDisable(GL_LIGHTING);
						if (mi == grabbedObject){
							glColor4f(0,1,0,1);
						} else if (fmi_selectable(mi)){
							glColor4f(1,1,1,1);
						} else {
							glColor4f(0,0,0,1);
						}
						
						for (int i = 0; i < mi->model->materialCount; i++){
							glDrawArrays(GL_TRIANGLES,mi->model->objects[0].vertexOffsetCounts[i].offset,mi->model->objects[0].vertexOffsetCounts[i].count);
						}

						glEnable(GL_TEXTURE_2D);
						glColor4f(1,1,1,1);
						glDisable(GL_STENCIL_TEST);

						glDisableClientState(GL_VERTEX_ARRAY);
						glDisableClientState(GL_NORMAL_ARRAY);
						glDisableClientState(GL_TEXTURE_COORD_ARRAY);
					}
				}
			}

			for (Fragment *f = fragments; f < fragments+COUNT(fragments); f++){
				if (f->model){
					FSRect rect;
					get_rect(f->position,&rect);
					
					glLoadIdentity();
					glTranslatef(slideX,0,0);
					glTranslated(rect.x+0.5f*rect.width,rect.y+0.5f*rect.height,10);
					glRotated(t0*120+f->rotationRandom,0,1,0);
					glScaled(0.75f,0.75f,0.75f);

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

			glLoadIdentity();
			glBindTexture(GL_TEXTURE_2D,font.id);
			set_font_height(1.0f);
			draw_text_shadow_centered_alpha(8.0f,4.5f,20,zoneTitleAlpha,"Zone 1: The Beach");

			glDisable(GL_TEXTURE_2D);
		}

		glLoadIdentity();
		if (fadeState != FADE_NONE){
			if (fadeState == FADE_OUT){
				fadeAlpha += dt;
				if (fadeAlpha >= 1.0f){
					fadeAlpha = 1.0f;
					gameState = targetGameState;
					fadeState = FADE_IN;
				}
			} else if (fadeState == FADE_IN){
				fadeAlpha -= dt;
				if (fadeAlpha <= 0.0f){
					fadeAlpha = 0.0f;
					fadeState = FADE_NONE;
				}
			}
			glColor4f(0,0,0,fadeAlpha);
			glBegin(GL_QUADS);
			glVertex3f(0,0,25);
			glVertex3f(16,0,25);
			glVertex3f(16,9,25);
			glVertex3f(0,9,25);
			glEnd();
		}

		glCheckError();
		alCheckError();
		alcCheckError(alcDevice);
 
		glfwSwapBuffers(gwindow);

		justClicked = false;

		POLL:
		glfwPollEvents();
	}
 
	cleanup();
}