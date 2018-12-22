/*
musicvisualizer base code
by Christian Eckhardt 2018
with some code snippets from Ian Thomas Dunn and Zoe Wood, based on their CPE CSC 471 base code
On Windows, it whould capture "what you here" automatically, as long as you have the Stereo Mix turned on!! (Recording devices -> activate)
*/

#include <iostream>
#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "GLSL.h"
#include "Program.h"
#include "MatrixStack.h"
#include "recordAudio.h"
#include "WindowManager.h"
#include "Shape.h"
// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <thread>
using namespace std;
using namespace glm;
shared_ptr<Shape> shape;
shared_ptr<Shape> box; //edit
shared_ptr<Shape> sphere; //edit

#define MESHSIZE 1000
extern captureAudio actualAudioData;
extern int running;

int renderstate = 1;//2..grid

#define TEXSIZE 256
BYTE texels[TEXSIZE*TEXSIZE*4];

#define RADIUS 6
# define M_PI 3.14159265358979323846
//********************
#include <math.h>
#include <algorithm>    


#include "kiss_fft.h"



#define FFTW_ESTIMATEE (1U << 6)
#define FFT_MAXSIZE 500

bool fft(float *amplitude_on_frequency,int &length)
{
	int N = pow(2, 10);
	BYTE data[MAXS];
	int size = 0;
	actualAudioData.readAudio(data, size);
	length = size / 8;
	if (size == 0)
		return false;

	double *samples = new double[length];
	for (int ii = 0; ii < length; ii++)
	{
		float *f = (float*)&data[ii * 8];
		samples[ii] = (double)(*f);
	}
	

	kiss_fft_cpx *cx_in = new kiss_fft_cpx[length];
	kiss_fft_cpx *cx_out = new kiss_fft_cpx[length];
	kiss_fft_cfg cfg = kiss_fft_alloc(length, 0, 0, 0);
	for (int i = 0; i<length; ++i) 
		{		
		cx_in[i].r = samples[i];
		cx_in[i].i = 0;
		}

		kiss_fft(cfg, cx_in, cx_out);

		float amplitude_on_frequency_old[FFT_MAXSIZE];
		for (int i = 0; i < length / 2 && i<FFT_MAXSIZE; ++i)
			amplitude_on_frequency_old[i] = amplitude_on_frequency[i];

		for (int i = 0; i < length/2 && i<FFT_MAXSIZE; ++i)
			amplitude_on_frequency[i] = sqrt(pow(cx_out[i].i,2.) + pow(cx_out[i].r, 2.));


		//that looks better, decomment for no filtering: +++++++++++++++++++
		for (int i = 0; i < length / 2 && i < FFT_MAXSIZE; ++i)
				{
				float diff = amplitude_on_frequency_old[i] - amplitude_on_frequency[i];
				float attack_factor = 0.1;//for going down
				if (amplitude_on_frequency_old[i] < amplitude_on_frequency[i]) 
					attack_factor = 0.85; //for going up
				diff *= attack_factor;
				amplitude_on_frequency[i] = amplitude_on_frequency_old[i] - diff;
				}			
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


		length /= 2;
		free(cfg);

	return true;
}
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

BYTE delayfilter(BYTE old, BYTE actual, float mul)
{
	float fold = (float)old;
	float factual = (float)actual;
	float fres = fold - (fold - factual) / mul;
	if (fres > 255) fres = 255;
	else if (fres < 0)fres = 0;
	return (BYTE)fres;
}

double get_last_elapsed_time()
{
	static double lasttime = glfwGetTime();
	double actualtime =glfwGetTime();
	double difference = actualtime- lasttime;
	lasttime = actualtime;
	return difference;
}
class camera
{
public:
	glm::vec3 pos, rot;
	int w, a, s, d, q, e, z, c;
	camera()
	{
		w = a = s = d = q = e = z = c = 0;
		pos = rot = glm::vec3(0, 0, 0);
	}
	glm::mat4 process(double ftime)
	{
		float speed = 0;
		if (w == 1)
		{
			speed = 90*ftime;
		}
		else if (s == 1)
		{
			speed = -90*ftime;
		}
		float yangle=0;
		if (a == 1)
			yangle = -3*ftime;
		else if(d==1)
			yangle = 3*ftime;
		rot.y += yangle;
		float zangle = 0;
		if (q == 1)
			zangle = -3 * ftime;
		else if (e == 1)
			zangle = 3 * ftime;
		rot.z += zangle;
		float xangle = 0;
		if (z == 1)
			xangle = -0.3 * ftime;
		else if (c == 1)
			xangle = 0.3 * ftime;
		rot.x += xangle;

		glm::mat4 R = glm::rotate(glm::mat4(1), rot.y, glm::vec3(0, 1, 0));
		glm::mat4 Rz = glm::rotate(glm::mat4(1), rot.z, glm::vec3(0, 0, 1));
		glm::mat4 Rx = glm::rotate(glm::mat4(1), rot.x, glm::vec3(1, 0, 0));
		glm::vec4 dir = glm::vec4(0, 0, speed,1);
		R = Rx * Rz * R;
		dir = dir*R;
		pos += glm::vec3(dir.x, dir.y, dir.z);
		glm::mat4 T = glm::translate(glm::mat4(1), pos);
		return R*T;
	}
};

camera mycam;

class Application : public EventCallbacks
{

public:

	WindowManager * windowManager = nullptr;

	// Our shader program
	std::shared_ptr<Program> prog;

	// Contains vertex information for OpenGL
	GLuint VertexArrayID;

	// Data necessary to give our box to OpenGL
	GLuint MeshPosID, MeshTexID;
	// FFT arrays
	float amplitude_on_frequency[FFT_MAXSIZE];
	float amplitude_on_frequency_10steps[10];



	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
		
		if (key == GLFW_KEY_W && action == GLFW_PRESS)
		{
			mycam.w = 1;
		}
		if (key == GLFW_KEY_W && action == GLFW_RELEASE)
		{
			mycam.w = 0;
		}
		if (key == GLFW_KEY_S && action == GLFW_PRESS)
		{
			mycam.s = 1;
		}
		if (key == GLFW_KEY_S && action == GLFW_RELEASE)
		{
			mycam.s = 0;
		}
		if (key == GLFW_KEY_A && action == GLFW_PRESS)
		{
			mycam.a = 1;
		}
		if (key == GLFW_KEY_A && action == GLFW_RELEASE)
		{
			mycam.a = 0;
		}
		if (key == GLFW_KEY_D && action == GLFW_PRESS)
		{
			mycam.d = 1;
		}
		if (key == GLFW_KEY_D && action == GLFW_RELEASE)
		{
			mycam.d = 0;
		}
		if (key == GLFW_KEY_Q && action == GLFW_PRESS)
		{
			mycam.q = 1;
		}
		if (key == GLFW_KEY_Q && action == GLFW_RELEASE)
		{
			mycam.q = 0;
		}
		if (key == GLFW_KEY_E && action == GLFW_PRESS)
		{
			mycam.e = 1;
		}
		if (key == GLFW_KEY_E && action == GLFW_RELEASE)
		{
			mycam.e = 0;
		}
		if (key == GLFW_KEY_Z && action == GLFW_PRESS)
		{
			mycam.z = 1;
		}
		if (key == GLFW_KEY_Z && action == GLFW_RELEASE)
		{
			mycam.z = 0;
		}
		if (key == GLFW_KEY_C && action == GLFW_PRESS)
		{
			mycam.c = 1;
		}
		if (key == GLFW_KEY_C && action == GLFW_RELEASE)
		{
			mycam.c = 0;
		}
		
	}

	// callback for the mouse when clicked move the triangle when helper functions
	// written
	void mouseCallback(GLFWwindow *window, int button, int action, int mods)
	{
	
	}

	//if the window is resized, capture the new size and reset the viewport
	void resizeCallback(GLFWwindow *window, int in_width, int in_height)
	{
		//get the window size - may be different then pixels for retina
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
	}

	/*Note that any gl calls must always happen after a GL state is initialized */
	void initGeom()
	{
		string resourceDirectory = "../resources" ;
		// Initialize mesh.
		shape = make_shared<Shape>();
		box = make_shared<Shape>();
		sphere = make_shared<Shape>();

		
		//sphere was changed to box ya dig

		shape->loadMesh(resourceDirectory + "/bunny.obj");
		shape->resize();
		shape->init();


		//this is for the center spehere
		sphere->loadMesh(resourceDirectory + "/sphere_all.obj");
		sphere->resize();
		sphere->init();

		//using cube.obj for the bar graph 
		box->loadMesh(resourceDirectory + "/cube.obj");
		box->resize();
		box->init();

		//generate the VAO
		glGenVertexArrays(1, &VertexArrayID);
		glBindVertexArray(VertexArrayID);

		//generate vertex buffer to hand off to OGL
		glGenBuffers(1, &MeshPosID);
		glBindBuffer(GL_ARRAY_BUFFER, MeshPosID);
		vec3 vertices[FFT_MAXSIZE];
		float steps = 7. / (float)FFT_MAXSIZE; //edit steps changed from 10. to 7

		for (int i = 0; i < FFT_MAXSIZE; i++)
			vertices[i] = vec3(get_x(i) , 0.0, get_z(i));
	
				
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * FFT_MAXSIZE, vertices, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glBindVertexArray(0);

	}

	
	//General OGL initialization - set OGL state here
	void init(const std::string& resourceDirectory)
	{
		GLSL::checkVersion();

		// Set background color.
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);
		
		// Initialize the GLSL program.
		prog = std::make_shared<Program>();
		prog->setVerbose(true);
		prog->setShaderNames(resourceDirectory + "/shader_vertex.glsl", resourceDirectory + "/shader_fragment.glsl");
		if (!prog->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		prog->addUniform("P");
		prog->addUniform("V");
		prog->addUniform("M");
		prog->addUniform("colorext");
		prog->addAttribute("vertPos");
		prog->addAttribute("vertNor");
		prog->addAttribute("vertTex");

		
		
	}
//******************** Functions to get coordinates for circlular bar graph

	float get_x(int i) {
		//i represents the angle of the circle
		double angle = i;
		while (angle > 360)
		{
			angle = angle - 360;
		}
		double x = RADIUS * cos(angle * M_PI / 180);
		return x;
	}

	float get_z(int i) {
		double angle = i;

		while (angle > 360)
		{
			angle = angle - 360;
		}
		double z = RADIUS * sin(angle * M_PI / 180);
		return z;
	}

//******************* COME BACK HERE YA DIGGG
	void aquire_fft_scaling_arrays()
		{
		//get FFT array
		static int length = 0;
		if (fft(amplitude_on_frequency, length))
			{
			//put the height of the frequencies 20Hz to 20000Hz into the height of the line-vertices
			vec3 vertices[FFT_MAXSIZE];
			glBindBuffer(GL_ARRAY_BUFFER, MeshPosID);
			//changed steps from 10 to 5
			float steps = 5. / (float)FFT_MAXSIZE;
			for (int i = 0; i < FFT_MAXSIZE; i++)
				{
				float height = 0;
				if (i < length)
					height = amplitude_on_frequency[i] * 0.05;
				//vertices[i] = vec3(-5.0 + i * steps, height, 0.0);
				vertices[i] = vec3(get_x(i), height, get_z(i));
				}
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * FFT_MAXSIZE, vertices);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			//calculate the average amplitudes for the 10 spheres

			//changed mean_range from length /10 to length / 5
			int mean_range = length / 5;
			int bar = 0;
			int count = 0;
			for (int i = 0; ; i++, count++)
				{
				if (mean_range == count)
					{
					count = -1;
					amplitude_on_frequency_10steps[bar] /= (float)mean_range;
					bar++;
					//changed it from 10 bars to 5 bars
					if (bar == 5)break;
					}
				if (i<length)
					amplitude_on_frequency_10steps[bar] += amplitude_on_frequency[i];
				}
			}
		}

	/****DRAW
	This is the most important function in your program - this is where you
	will actually issue the commands to draw any geometry you have set up to
	draw
	********/
	void render()
	{
		static double count = 0;
		double frametime = get_last_elapsed_time();
		count += frametime;

		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		float aspect = width/(float)height;
		glViewport(0, 0, width, height);

		// Clear framebuffer.
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 V, M, P; //View, Model and Perspective matrix
		
		V = mycam.process(frametime);
		P = glm::perspective((float)(3.14159 / 4.), (float)((float)width/ (float)height), 0.01f, 100000.0f); //so much type casting... GLM metods are quite funny ones
				
		
		// Draw the box using GLSL.
		prog->bind();
		
		//Set the FFT arrays
		aquire_fft_scaling_arrays();

		//send the matrices to the shaders
		
		glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
			
		//first box
		
		float scaling = amplitude_on_frequency_10steps[0] * 0.8;
		vec3 color1 = vec3(1 * scaling, (float)120 * scaling / 255, (float)100 / 255.);
		glUniform3fv(prog->getUniform("colorext"), 1, &color1.x);
		if ((0.2 + scaling) > 0)
			M = glm::translate(glm::mat4(1.0f), vec3(0, 2+ scaling, -5)) *  glm::scale(mat4(1), vec3(0.2, 0.2 + scaling, 0.2));
		//glm::rotate(glm::mat(1.0f), vec3(5, 2, 10)) *
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		shape->draw(prog, FALSE);

		//second box
		float scaling2 = amplitude_on_frequency_10steps[1] * 0.8;
		vec3 color2 = vec3(1* scaling2, (float)120*scaling2 / 255, (float)1 / 255.);
		glUniform3fv(prog->getUniform("colorext"), 1, &color2.x);
		if ((0.2 + scaling2) > 0)
			M = glm::translate(glm::mat4(1.0f), vec3(-5, 2 + scaling2, 0)) * glm::scale(mat4(1), vec3(0.2, 0.2 + scaling2, 0.2));
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		shape->draw(prog, FALSE);

		//third box
		float scaling3 = amplitude_on_frequency_10steps[3] * 1;
		vec3 color3 = vec3(1 * scaling3, (float)120 * scaling3 / 255, (float)50 / 255.);
		glUniform3fv(prog->getUniform("colorext"), 1, &color3.x);
		if ((0.2 + scaling3) > 0)
			M = glm::translate(glm::mat4(1.0f), vec3(0, 2 + scaling3, 5)) * glm::scale(mat4(1), vec3(0.2, 0.2 + scaling3, 0.2));
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		shape->draw(prog, FALSE);

		//4th box
		float scaling4 = amplitude_on_frequency_10steps[3] * 0.8;
		vec3 color4 = vec3(1 * scaling4, (float)120 * scaling4 / 255, (float)150 / 255.);
		glUniform3fv(prog->getUniform("colorext"), 1, &color4.x);	
		if((0.2 + scaling4)>0)
			M = glm::translate(glm::mat4(1.0f), vec3(5, 2+ scaling4, 0)) * glm::scale(mat4(1), vec3(0.2, 0.2 + scaling4, 0.2));
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		shape->draw(prog, FALSE);

		//sphere
		//color values should be between 0 and 1
		/*float scaling5 = 0;
		for (int i = 0; i < 5; i++)
		{
			scaling5 += amplitude_on_frequency_10steps[i];
		}
		scaling5 = (scaling5 / 4) * 0.2;
		*/
		float scaling5 = (scaling + + scaling2 + scaling3 + scaling4)/4;
		vec3 color5 = vec3(1 * scaling5, (float)120 * scaling5/ 255, (float)200 / 255.);
		// use clamp function if this doesnt work
		glUniform3fv(prog->getUniform("colorext"), 1, &color5.x);
		M =glm::translate(glm::mat4(1.0f), vec3(0, 1 + scaling5,0)) * glm::scale(mat4(1), vec3(0.9,  scaling5, 0.9));
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		sphere->draw(prog, FALSE);


			//this is for the bar graph

		for (int i = 0; i < FFT_MAXSIZE; i++)
		{
		
			vec3 bcolor = vec3(.9, 0.3, (i + 100) /600 );
			glUniform3fv(prog->getUniform("colorext"), 1, &bcolor.x);
			M = glm::translate(glm::mat4(1.0f), vec3(get_x(i), 2 + amplitude_on_frequency[i] * 0.05, get_z(i))) * glm::scale(mat4(1), vec3(0.01, 0.01 + 0.05 * amplitude_on_frequency[i], 0.01));
			glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			box->draw(prog, FALSE);
		}
		for (double i2 = 0.01; i2 < 0.05; i2 += 0.01)
		{
			for (int i = 0; i < FFT_MAXSIZE; i++)
			{

				vec3 bcolor = vec3(1., 1., 1.);
				glUniform3fv(prog->getUniform("colorext"), 1, &bcolor.x);
				M = glm::translate(glm::mat4(1.0f), vec3(get_x(i), 2 + i2 + (amplitude_on_frequency[i] * 0.05), get_z(i))) * glm::scale(mat4(1), vec3(0.01, 0.01, 0.01));
				glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
				box->draw(prog, FALSE);
			}
		}
		
	}
};


//******************************************************************************************
int main(int argc, char **argv)
{
	std::string resourceDir = "../resources"; // Where the resources are loaded from
	if (argc >= 2)
	{
		resourceDir = argv[1];
	}

	Application *application = new Application();

	/* your main will always include a similar set up to establish your window
		and GL context, etc. */
	WindowManager * windowManager = new WindowManager();
	windowManager->init(1920, 1080);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	/* This is the code that will likely change program to program as you
		may need to initialize or set up different data and state */
	// Initialize scene.
	application->init(resourceDir);
	application->initGeom();

	thread t1(start_recording);
	// Loop until the user closes the window.
	while(! glfwWindowShouldClose(windowManager->getHandle()))
	{
		// Render scene.
		application->render();
		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
	}
	running = FALSE;
	t1.join();

	// Quit program.
	windowManager->shutdown();
	return 0;
}
