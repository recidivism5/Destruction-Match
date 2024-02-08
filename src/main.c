#include <glutil.h>
#include <alutil.h>

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

Texture beachBackground, checker, frame;

FracturedModel models[3];

FracturedModelInstance *grabbedObject;

EmptyPair pairs[128];
int pairCount;

FracturedModelInstance *marked[64*2];
int markedCount;

Move move;

////////////////END GLOBALS

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
					(float)((rand_int(2) ? -1 : 1) * rand_int_range(5,10)),
					(float)(rand_int_range(5,10))
				},
				object->rotationRandom
			);
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

void cleanup(void){
	glfwDestroyWindow(gwindow);
	glfwTerminate();
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
							if (si->state != SETTLED || di->state != SETTLED){
								break;
							}
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
	gen_font_atlas(&font,"Nunito-Regular",36);

	load_texture(&beachBackground,"campaigns/juicebar/textures/background.jpg",true);
	load_texture(&checker,"textures/checker.png",false);
	load_texture(&frame,"campaigns/juicebar/textures/frame.png",true);

	load_fractured_model(models+0,"campaigns/juicebar/models/apple");
	load_fractured_model(models+1,"campaigns/juicebar/models/banana");
	load_fractured_model(models+2,"campaigns/juicebar/models/orange");

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

	//Loop:

	double t0 = glfwGetTime();
 
	while (!glfwWindowShouldClose(gwindow)){
		/////////// Update:
		double t1 = glfwGetTime();
		float dt = (float)(t1 - t0);
		t0 = t1;

		//dt *= 0.25f;

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
							append_pair(x,y,0);
						}
					}
				}
				//pick random pair and make a potential match with it:
				{
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
		for (int x = 0; x < 8; x++){
			for (int y = 0; y < 8; y++){
				FracturedModelInstance *mi = &board[x][y];
				if (mi->state == UNPLACED){
					mi->position[0] = boardRect.left + x*cellWidth;
					mi->position[1] = boardRect.top + (y+1)*cellWidth*2;
					mi->rotationRandom = (float)rand_int(360);
					mi->yVelocity = 0.0f;
					mi->state = FALLING;
				} else if (mi->state == FALLING){
					mi->yVelocity += -9.8f * dt;
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
		glScissor((int)screen.x,(int)screen.y,(int)screen.width,(int)screen.height);
		glEnable(GL_SCISSOR_TEST);

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

			for (int x = 0; x < 8; x++){
				for (int y = 0; y < 8; y++){
					FracturedModelInstance *mi = &board[x][y];
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
						if (mi == grabbedObject){
							glColor4f(0,1,0,1);
						} else if (fmi_selectable(mi)){
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
		alCheckError();
		alcCheckError(alcDevice);
 
		glfwSwapBuffers(gwindow);

		POLL:
		glfwPollEvents();
	}
 
	cleanup();
}