#include <glutil.h>
#include <perlin_noise.h>

Texture beachBackground, checker, frame;

FracturedModel banana;

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

FSRect screen;
void sub_viewport(float x, float y, float width, float height){
	glViewport(
		(int)(screen.x + screen.width * x / 16.0f),
		(int)(screen.y + screen.height * y / 9.0f),
		(int)(screen.width * width / 16.0f),
		(int)(screen.height * height / 9.0f)
	);
}

void main(void){
	glfwSetErrorCallback(error_callback);
 
	ASSERT(glfwInit());
 
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_SAMPLES, 4);
	GLFWwindow *window = create_centered_window(1280,960,"Match Mayhem");
 
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetKeyCallback(window, key_callback);
 
	glfwMakeContextCurrent(window);
	gladLoadGL();
	glfwSwapInterval(1);

	srand((unsigned int)time(0));

	//Init:
	load_texture(&beachBackground,"campaigns/juicebar/textures/background.jpg",true);
	load_texture(&checker,"textures/checker.png",false);
	load_texture(&frame,"campaigns/juicebar/textures/frame.png",true);

	load_fractured_model(&banana,"campaigns/juicebar/models/banana");

	//Loop:

	double t0 = glfwGetTime();
 
	while (!glfwWindowShouldClose(window))
	{
		/////////// Update:
		double t1 = glfwGetTime();
		double dt = t1 - t0;
		t0 = t1;

		////////////// Render:
		int clientWidth, clientHeight;
		glfwGetFramebufferSize(window, &clientWidth, &clientHeight);
		if (!clientWidth || !clientHeight){
			goto POLL;
		}
		screen.height = (float)clientWidth * 9.0f / 16.0f;
		if (screen.height > (float)clientHeight){
			screen.width = (float)clientHeight * 16.0f / 9.0f;
			screen.x = 0.5f * (clientWidth - screen.width);
			screen.y = 0;
		} else {
			screen.width = (float)clientWidth;
			screen.x = 0;
			screen.y = 0.5f*(clientHeight - screen.height);
		}
		glViewport((int)screen.x,(int)screen.y,(int)screen.width,(int)screen.height);

		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		glShadeModel(GL_SMOOTH);

		glEnable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);

		float vpad = 9 * 0.15f;
		float hw = 0.5f * (9 - 2*vpad);
		float fhw = hw * 1.33f;
		float cellWidth = hw/4;
		vec2 center = {16 * 2.0f / 3.0f,vpad + hw};
		FRect board = {
			.left = center[0]-hw,
			.right = center[0]+hw,
			.bottom = center[1]-hw,
			.top = center[1]+hw
		};
		vec2 fcenter = {center[0]-hw*0.014f,center[1]+hw*0.014f};

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
			glTexCoord2f(0,0); glVertex3f(board.left,board.bottom,1);
			glTexCoord2f(4,0); glVertex3f(board.right,board.bottom,1);
			glTexCoord2f(4,4); glVertex3f(board.right,board.top,1);
			glTexCoord2f(0,4); glVertex3f(board.left,board.top,1);
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

			for (int y = 0; y < 8; y++){
				for (int x = 0; x < 8; x++){
					sub_viewport(
						board.left+x*cellWidth,
						board.bottom+y*cellWidth,
						cellWidth,
						cellWidth
					);

					glLoadIdentity();
					glTranslated(0,0,-2.6);
					glRotated(t0*120+140*x-70*y,0,1,0);

					glBindTexture(GL_TEXTURE_2D,banana.materials[0].textureId);

					glEnableClientState(GL_VERTEX_ARRAY);
					glEnableClientState(GL_NORMAL_ARRAY);
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glEnable(GL_STENCIL_TEST);
					glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
					glStencilFunc(GL_ALWAYS,1,0xff);
					glStencilMask(0xff);
					glEnable(GL_LIGHTING);
					glVertexPointer(3,GL_FLOAT,sizeof(ModelVertex),(void *)&banana.vertices->position);
					glNormalPointer(GL_FLOAT,sizeof(ModelVertex),(void *)&banana.vertices->normal);
					glTexCoordPointer(2,GL_FLOAT,sizeof(ModelVertex),(void *)&banana.vertices->texcoord);
					glDrawArrays(GL_TRIANGLES,banana.objects[0].vertexOffsetCounts[0].offset,banana.objects[0].vertexOffsetCounts[0].count);
					glStencilFunc(GL_NOTEQUAL,1,0xff);
					glVertexPointer(3,GL_FLOAT,sizeof(vec3),(void *)banana.expandedPositions);
					glDisable(GL_TEXTURE_2D);
					glDisable(GL_LIGHTING);
					glColor4f(0,0,0,1);
					glDrawArrays(GL_TRIANGLES,banana.objects[0].vertexOffsetCounts[0].offset,banana.objects[0].vertexOffsetCounts[0].count);
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
 
		glfwSwapBuffers(window);

		POLL:
		glfwPollEvents();
	}
 
	glfwDestroyWindow(window);
 
	glfwTerminate();
}