#include <renderer.h>

Texture spriteTexture;
GPUMesh sprites;
TextureVertexList spriteVerts;
int gen_sprite(int x, int y, int width, int height){
	TextureVertex *v = TextureVertexListMakeRoom(&spriteVerts,6);
	float hw = (float)width/2.0f, hh = (float)height/2.0f;
	float fx = (float)x/spriteTexture.width, fy = (float)y/spriteTexture.height;
	float fw = (float)width/spriteTexture.width, fh = (float)height/spriteTexture.height;
	v[0].position[0] = -hw;
	v[0].position[1] = hh;
	v[0].position[2] = 0.0f;
	v[0].texcoord[0] = fx;
	v[0].texcoord[1] = fy;

	v[1].position[0] = -hw;
	v[1].position[1] = -hh;
	v[1].position[2] = 0.0f;
}
void finalize_sprites(){
	
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
					GLFWvidmode *vm = glfwGetVideoMode(primary);
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
	float fp = 4*M_PI;
	for (int i = 0; i < 3; i++){
		if (e[i] > fp) e[i] -= fp;
		else if (e[i] < -fp) e[i] += fp;
	}
}
void rotate_euler(vec3 e, float dx, float dy, float sens){
	e[1] += sens * dx;
	e[0] += sens * dy;
	clamp_euler(e);
}
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos){
	//rotate_euler(player.head_euler,xpos,ypos,-0.001f);
	glfwSetCursorPos(window, 0, 0);
}

GLFWwindow *create_centered_window(int width, int height, char *title){
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	GLFWwindow *window = glfwCreateWindow(width,height,title,NULL,NULL);
	ASSERT(window);
	GLFWmonitor *primary = glfwGetPrimaryMonitor();
	ASSERT(primary);
	GLFWvidmode *vm = glfwGetVideoMode(primary);
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

	compile_shaders();

	texture_from_file(&spriteTexture,"sprites");

	srand(time(0));

	double t0 = glfwGetTime();
 
	while (!glfwWindowShouldClose(window))
	{
		double t1 = glfwGetTime();
		double dt = t1 - t0;
		t0 = t1;

		int width,height;
		glfwGetFramebufferSize(window, &width, &height);

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
 
		glClearColor(57/255.0f, 130/255.0f, 207/255.0f, 1.0f);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		//glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		mat4 persp;
		mat4 vp;
		glm_perspective(0.45f*M_PI,(float)width/(float)height,0.01f,100.0f,persp);
		glm_translate_make(vp,(vec3){0,0,-5});
		glm_mat4_mul(persp,vp,vp);

		/*glUseProgram(texture_shader.id);
		glBindTexture(GL_TEXTURE_2D,sprites.id);
		glUniformMatrix4fv(texture_shader.uMVP,1,GL_FALSE,(GLfloat *)vp);
		glUniform1i(texture_shader.uTex,0);

		GPUMesh m;
		TextureVertex verts[] = {
			
		};
		gpu_mesh_from_texture_verts(&m,verts,COUNT(verts));
		glDrawArrays(GL_TRIANGLES,0,m.vertex_count);
		delete_gpu_mesh(&m);*/

		glCheckError();
 
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
 
	glfwDestroyWindow(window);
 
	glfwTerminate();
}