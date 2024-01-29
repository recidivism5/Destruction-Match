#include <glutil.h>
#include <matrix_stack.h>
#include <aabb.h>
#include <grid.h>

FontAtlas singleDay;

GLuint phongShader,gridShader,skyboxShader,screenShader,backgroundShader;

GLuint beachBackground;

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
		glm_vec3_copy(position,bd->position);
		glm_vec3_zero(bd->velocity);
		glm_quat_identity(bd->quaternion);
		glm_vec3_zero(bd->angularVelocity);
	}
}

void error_callback(int error, const char* description)
{
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
			}
			break;
		}
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods){
	switch (action){
		case GLFW_PRESS:{
			switch (button){
			}
			break;
		}
		case GLFW_RELEASE:{
			switch (button){
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
 
	ASSERT(glfwInit());
 
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
   	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fucking apple
#endif
	glfwWindowHint(GLFW_SAMPLES, 4);
	GLFWwindow *window = create_centered_window(1280,720,"Match Mayhem");
 
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetKeyCallback(window, key_callback);
 
	glfwMakeContextCurrent(window);
	gladLoadGL();
	glfwSwapInterval(1);

	gen_font_atlas(&singleDay,"SingleDay-Regular",32);

	phongShader = load_shader("phong");
	gridShader = load_shader("grid");
	skyboxShader = load_shader("skybox");
	screenShader = load_shader("screen");
	backgroundShader = load_shader("background");

	beachBackground = load_texture("backgrounds/beach.jpg",true);

	load_fractured_model(&banana,"banana");

	for (int y = 0; y < 8; y++){
		for (int x = 0; x < 8; x++){
			add_model_instance(&banana,(vec3){0.5f+(float)x,0.5f+(float)y,-5.0f});
		}
	}

	srand((unsigned int)time(0));

	double t0 = glfwGetTime();
 
	while (!glfwWindowShouldClose(window))
	{
		/////////// Update:
		double t1 = glfwGetTime();
		double dt = t1 - t0;
		t0 = t1;

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

		{
			glDepthFunc(GL_LEQUAL);
			glUseProgram(backgroundShader);
			shader_set_int(backgroundShader,"uTex",0);
			glBindTexture(GL_TEXTURE_2D,beachBackground);
			glDrawArrays(GL_TRIANGLE_STRIP,0,4);
			glDepthFunc(GL_LESS);
		}

		{
			vec3 cameraPos = {0,0,0};
			glUseProgram(phongShader);
			shader_set_int(phongShader,"uTex",0);
			shader_set_vec3(phongShader,"cameraPos",cameraPos);
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
				glm_vec3_sub(mi->bodyDatas[0].position,cameraPos,t);
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
		}

		glCheckError();
 
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
 
	glfwDestroyWindow(window);
 
	glfwTerminate();
}