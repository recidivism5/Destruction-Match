#include <glutil.h>
#include <matrix_stack.h>

GLuint phongShader;

FracturedModel banana;

FracturedModelInstance modelInstances[65536];
int modelInstanceCount;

void add_model_instance(FracturedModel *model, vec3 position){
	ASSERT(modelInstanceCount < COUNT(modelInstances));
	FracturedModelInstance *mi = modelInstances + modelInstanceCount++;
	mi->model = model;
	mi->bodyDatas = malloc(model->objectCount * sizeof(*mi->bodyDatas));
	for (int i = 0; i < model->objectCount; i++){
		BodyData *bd = mi->bodyDatas + i;
		glm_vec3_copy(model->objects[i].position,bd->position);
		glm_vec3_zero(bd->velocity);
		glm_quat_identity(bd->quaternion);
		glm_vec3_zero(bd->angularVelocity);
	}
}

struct {
	AABB aabb;
	vec3 headEuler;
} player;

void get_player_head_position(vec3 p){
	p[0] = player.aabb.position[0];
	p[1] = player.aabb.position[1] + 0.7f;
	p[2] = player.aabb.position[2];
}

void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

struct {
	bool
		left,
		right,
		down,
		up,
		attack,
		just_attacked,
		interact,
		just_interacted;
} keys;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
	switch (action){
		case GLFW_PRESS:{
			switch (key){
				case GLFW_KEY_ESCAPE:{
					glfwSetWindowShouldClose(window, GLFW_TRUE);
					break;
				}
				case GLFW_KEY_A: keys.left = true; break;
				case GLFW_KEY_D: keys.right = true; break;
				case GLFW_KEY_S: keys.down = true; break;
				case GLFW_KEY_W: keys.up = true; break;
				case GLFW_KEY_R:{
					static int prev_width, prev_height;
					static bool fullscreen = false;
					GLFWmonitor *primary = glfwGetPrimaryMonitor();
					const GLFWvidmode *vm = glfwGetVideoMode(primary);
					if (!fullscreen){
						glfwGetFramebufferSize(window,&prev_width,&prev_height);
						glfwSetWindowMonitor(window,primary,0,0,vm->width,vm->height,GLFW_DONT_CARE);
						fullscreen = true;
					} else {
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
				case GLFW_KEY_A: keys.left = false; break;
				case GLFW_KEY_D: keys.right = false; break;
				case GLFW_KEY_S: keys.down = false; break;
				case GLFW_KEY_W: keys.up = false; break;
			}
			break;
		}
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods){
	switch (action){
		case GLFW_PRESS:{
			switch (button){
				case GLFW_MOUSE_BUTTON_LEFT: keys.attack = true; keys.just_attacked = true; break;
				case GLFW_MOUSE_BUTTON_RIGHT: keys.interact = true; keys.just_interacted = true; break;
			}
			break;
		}
		case GLFW_RELEASE:{
			switch (button){
				case GLFW_MOUSE_BUTTON_LEFT: keys.attack = false; break;
				case GLFW_MOUSE_BUTTON_RIGHT: keys.interact = false; keys.just_interacted = false; break;
			}
			break;
		}
	}
}

void clamp_euler(vec3 e){
	float fp = 4*(float)M_PI;
	for (int i = 0; i < 3; i++){
		if (e[i] > fp){
			e[i] -= fp;
		} else if (e[i] < -fp){
			e[i] += fp;
		}
	}
}
void rotate_euler(vec3 e, float dx, float dy, float sens){
	e[1] += sens * dx;
	e[0] += sens * dy;
	clamp_euler(e);
	e[0] = CLAMP(e[0],-0.5f*(float)M_PI,0.5f*(float)M_PI);
}
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos){
	rotate_euler(player.headEuler,(float)xpos,(float)ypos,-0.001f);
	glfwSetCursorPos(window, 0, 0);
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
 
	ASSERT(glfwInit());
 
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
   	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fucking apple
#endif
	GLFWwindow *window = create_centered_window(640,480,"Burger Land");
 
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported()){
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}
	glfwSetCursorPos(window, 0, 0);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetKeyCallback(window, key_callback);
 
	glfwMakeContextCurrent(window);
	gladLoadGL();
	glfwSwapInterval(1);

	phongShader = load_shader("phong");
	load_fractured_model(&banana,"banana");
	add_model_instance(&banana,(vec3){0,0,0});

	player.aabb.halfExtents[0] = 0.25f;
	player.aabb.halfExtents[1] = 0.9f;
	player.aabb.halfExtents[2] = 0.25f;

	srand((unsigned int)time(0));

	double t0 = glfwGetTime();
 
	while (!glfwWindowShouldClose(window))
	{
		/////////// Update:
		double t1 = glfwGetTime();
		double dt = t1 - t0;
		t0 = t1;

		ivec2 move_dir;
		if (keys.left && keys.right){
			move_dir[0] = 0;
		} else if (keys.left){
			move_dir[0] = -1;
		} else if (keys.right){
			move_dir[0] = 1;
		} else {
			move_dir[0] = 0;
		}
		if (keys.down && keys.up){
			move_dir[1] = 0;
		} else if (keys.down){
			move_dir[1] = 1;
		} else if (keys.up){
			move_dir[1] = -1;
		} else {
			move_dir[1] = 0;
		}

		mat4 crot, invCrot;
		glm_euler_yxz(player.headEuler,crot);
		glm_mat4_transpose_to(crot,invCrot);
		vec3 c_right,c_backward;
		glm_vec3(crot[0],c_right);
		glm_vec3(crot[2],c_backward);
		glm_vec3_scale(c_right,(float)move_dir[0],c_right);
		glm_vec3_scale(c_backward,(float)move_dir[1],c_backward);
		vec3 move_vec;
		glm_vec3_add(c_right,c_backward,move_vec);
		glm_vec3_normalize(move_vec);
		glm_vec3_scale(move_vec,10.0f*(float)dt,move_vec);
		glm_vec3_add(player.aabb.position,move_vec,player.aabb.position);

		////////////// Render:
		int width,height;
		glfwGetFramebufferSize(window, &width, &height);

		glClearColor(57/255.0f, 130/255.0f, 207/255.0f, 1.0f);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		ms_load_identity();
		ms_persp(0.45f*(float)M_PI,(float)width/(float)height,0.01f,100.0f);
		ms_mul(invCrot);

		vec3 playerHeadPos;
		get_player_head_position(playerHeadPos);

		glUseProgram(phongShader);
		shader_set_int(phongShader,"uTex",0);
		shader_set_vec3(phongShader,"cameraPos",playerHeadPos);
		shader_set_int(phongShader,"numLights",1);
		shader_set_vec3(phongShader,"lights[0].position",(vec3){3,3,3});
		shader_set_vec3(phongShader,"lights[0].color",(vec3){1,1,1});
		for (FracturedModelInstance *mi = modelInstances; mi < modelInstances+modelInstanceCount; mi++){
			glBindVertexArray(mi->model->vao);
			FracturedObject *obj = mi->model->objects;
			mat4 rot;
			glm_quat_mat4(mi->bodyDatas[0].quaternion,rot);
			ms_push();
			vec3 t;
			glm_vec3_sub(mi->bodyDatas[0].position,playerHeadPos,t);
			ms_trans(t);
			ms_mul(rot);
			shader_set_mat4(phongShader,"uMVP",ms_get(),false);
			ms_load_identity();
			ms_trans(mi->bodyDatas[0].position);
			ms_mul(rot);
			shader_set_mat4(phongShader,"uMTW",ms_get(),false);
			ms_pop();
			for (int i = 0; i < mi->model->materialCount; i++){
				Material *mat = mi->model->materials + i;
				glBindTexture(GL_TEXTURE_2D,mat->textureId);
				int shininess = 2;
				float target = 256.0f * (1.0f - mat->roughness);
				while ((float)shininess < target && shininess < 256){
					shininess *= 2;
				}
				shader_set_float(phongShader,"shininess",(float)shininess);
				VertexOffsetCount *vic = mi->model->objects[0].vertexOffsetCounts+i;
				glDrawArrays(GL_TRIANGLES,vic->offset,vic->count);
			}
		}

		glCheckError();
 
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
 
	glfwDestroyWindow(window);
 
	glfwTerminate();
}