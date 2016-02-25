#include "glew.h"		// include GL Extension Wrangler

#include "glfw3.h"  // include GLFW helper library

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "gtc/constants.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <cctype>
#include <sstream>

using namespace std;

#define M_PI        3.14159265358979323846264338327950288   /* pi */
#define DEG_TO_RAD	M_PI/180.0f

GLFWwindow* window = 0x00;

GLuint shader_program = 0;

GLuint view_matrix_id = 0;
GLuint model_matrix_id = 0;
GLuint proj_matrix_id = 0;


///Transformations
glm::mat4 proj_matrix;
glm::mat4 view_matrix;
glm::mat4 model_matrix;

float hermiteBasis[16] = {2, -2, 1, 1, -3, 3, -2, -1, 0, 0, 1, 0, 1, 0, 0, 0};
glm::mat4 hermiteBasisMatrix;

GLuint VBO, VAO, EBO;

GLfloat point_size = 3.0f;

const GLuint WIDTH = 800, HEIGHT = 600;

glm::vec3 mousePosition;

int numberOfPoints;
vector<glm::vec3> pointPositions;
vector<glm::vec3> tangentPositions;

vector<glm::vec3> lines;

GLfloat* g_vertex_buffer_data;

// Helper function to convert vec3's into a formatted string
string vec3tostring(glm::vec3 vec)
{
	std::stringstream ss;
	ss << vec.x << ' ' << vec.y << ' ' << vec.z;
	return ss.str();
}

///Handle the keyboard input
void keyPressed(GLFWwindow *_window, int key, int scancode, int action, int mods) {
	switch (key) {
	default: break;
	}
	return;
}

glm::vec3* myMat1x4Multiply(glm::vec4 &paramBasis, glm::mat3x4 &control)
{
	return new glm::vec3(glm::dot(paramBasis,(control)[0]), glm::dot(paramBasis,(control)[1]), glm::dot(paramBasis,(control)[2]));
}

glm::vec4* anotherFunkyM(glm::vec4 &param, glm::mat4 &basis)
{
	glm::mat4 tmp(glm::transpose(basis));
	return new glm::vec4(glm::dot(param, tmp[0]), glm::dot(param, tmp[1]), glm::dot(param, tmp[2]), glm::dot(param, tmp[3]));
}

void mouseClick(GLFWwindow *_window, int button, int action, int mods)
{
	if (action != GLFW_PRESS)
	{
		return;
	}

	if (pointPositions.size() < numberOfPoints)
	{
		pointPositions.push_back(glm::vec3(mousePosition));
	}
	else if (tangentPositions.size() < numberOfPoints)
	{
		tangentPositions.push_back(glm::vec3(mousePosition));
	}

	cout << "Point positions:" << endl;

	for (unsigned i = 0; i < pointPositions.size(); i++)
	{
		cout << vec3tostring(pointPositions[i]) << endl;
	}

	cout << endl;
}

void mousePositionCallback(GLFWwindow *_window, double xpos, double ypos)
{
	//cout << "x: " << xpos << " - y: " << ypos << endl;
	int w, h;
	glfwGetWindowSize(_window, &w, &h);
	mousePosition = glm::unProject(glm::vec3((float)xpos, (float)ypos, 0.0f), model_matrix, proj_matrix, glm::vec4(0, 0, w, h));
	mousePosition.y = -mousePosition.y;

	//cout << vec3tostring(mousePosition) << endl;
}


bool initialize() {
	/// Initialize GL context and O/S window using the GLFW helper library
	if (!glfwInit()) {
		fprintf(stderr, "ERROR: could not start GLFW3\n");
		return false;
	}

	/// Create a window of size 640x480 and with title "Lecture 2: First Triangle"
	glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);
	window = glfwCreateWindow(WIDTH, HEIGHT, "COMP371: Assignment 2", NULL, NULL);
	if (!window) {
		fprintf(stderr, "ERROR: could not open window with GLFW3\n");
		glfwTerminate();
		return false;
	}

	glViewport(0, 0, WIDTH, HEIGHT);
	proj_matrix = glm::ortho(-1, 1, -1, 1);
	//view_matrix = glm::vec4(0, 0, WIDTH, HEIGHT);

	int w, h;
	glfwGetWindowSize(window, &w, &h);
	///Register the keyboard callback function: keyPressed(...)
	glfwSetKeyCallback(window, keyPressed);
	glfwSetMouseButtonCallback(window, mouseClick);
	glfwSetCursorPosCallback(window, mousePositionCallback);

	glfwMakeContextCurrent(window);

	/// Initialize GLEW extension handler
	glewExperimental = GL_TRUE;	///Needed to get the latest version of OpenGL
	glewInit();

	/// Get the current OpenGL version
	const GLubyte* renderer = glGetString(GL_RENDERER); /// Get renderer string
	const GLubyte* version = glGetString(GL_VERSION); /// Version as a string
	printf("Renderer: %s\n", renderer);
	printf("OpenGL version supported %s\n", version);

	/// Enable the depth test i.e. draw a pixel if it's closer to the viewer
	glEnable(GL_DEPTH_TEST); /// Enable depth-testing
	glDepthFunc(GL_LESS);	/// The type of testing i.e. a smaller value as "closer"

	hermiteBasisMatrix = glm::make_mat4(hermiteBasis);

	return true;
}

bool cleanUp() {
	glDisableVertexAttribArray(0);
	//Properly de-allocate all resources once they've outlived their purpose
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);

	// Close GL context and any other GLFW resources
	glfwTerminate();

	return true;
}

GLuint loadShaders(std::string vertex_shader_path, std::string fragment_shader_path) {
	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_shader_path, std::ios::in);
	if (VertexShaderStream.is_open()) {
		std::string Line = "";
		while (getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}
	else {
		printf("Impossible to open %s. Are you in the right directory ? Don't forget to read the FAQ !\n", vertex_shader_path.c_str());
		getchar();
		exit(-1);
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_shader_path, std::ios::in);
	if (FragmentShaderStream.is_open()) {
		std::string Line = "";
		while (getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_shader_path.c_str());
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer, nullptr);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(VertexShaderID, InfoLogLength, nullptr, &VertexShaderErrorMessage[0]);
		printf("%s\n", &VertexShaderErrorMessage[0]);
	}

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_shader_path.c_str());
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, nullptr);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		std::vector<char> FragmentShaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(FragmentShaderID, InfoLogLength, nullptr, &FragmentShaderErrorMessage[0]);
		printf("%s\n", &FragmentShaderErrorMessage[0]);
	}

	// Link the program
	printf("Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);

	glBindAttribLocation(ProgramID, 0, "in_Position");

	//appearing in the vertex shader.
	glBindAttribLocation(ProgramID, 1, "in_Color");

	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
		glGetProgramInfoLog(ProgramID, InfoLogLength, nullptr, &ProgramErrorMessage[0]);
		printf("%s\n", &ProgramErrorMessage[0]);
	}

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	//The three variables below hold the id of each of the variables in the shader
	//If you read the vertex shader file you'll see that the same variable names are used.
	view_matrix_id = glGetUniformLocation(ProgramID, "view_matrix");
	model_matrix_id = glGetUniformLocation(ProgramID, "model_matrix");
	proj_matrix_id = glGetUniformLocation(ProgramID, "proj_matrix");

	return ProgramID;
}


int main() {

	

	cout << "How many points would you like to enter?" << endl;
	cin >> numberOfPoints;


	initialize();

	///Load the shaders
	shader_program = loadShaders("COMP371_hw1.vs", "COMP371_hw1.fs");

	// This will identify our vertex buffer
	GLuint vertexbuffer;

	
	// Generate 1 buffer, put the resulting identifier in vertexbuffer
	glGenBuffers(1, &vertexbuffer);

	// The following commands will talk about our 'vertexbuffer' buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	// Give our vertices to OpenGL.
	
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		3,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
		);


	bool first = false;

	
	
	pointPositions.push_back(glm::vec3(0, 0, 1));
	pointPositions.push_back(glm::vec3(0.5, 0, 1));
	tangentPositions.push_back(glm::vec3(0, 0.5, 1));
	tangentPositions.push_back(glm::vec3(0.5, 0.5, 1));
	


	while (!glfwWindowShouldClose(window)) {
		// wipe the drawing surface clear
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.1f, 0.2f, 0.2f, 1.0f);
		glPointSize(point_size);

		glUseProgram(shader_program);

		//g_vertex_buffer_data[0] = mousePosition.x;
		//g_vertex_buffer_data[1] = mousePosition.y;

		//model_matrix = glm::translate(oriModel, mousePosition);

		
		if (tangentPositions.size() > 1 && !first)
		{
			first = true;

			lines.empty();

			float u = 0;
			while (u <= 1)
			{
				glm::vec4 param(u*u*u, u*u, u, 1);
				//glm::vec4 param(1, u, u*u, u*u*u);
				glm::mat4 paramm(u*u*u, u*u, u, 1,0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0);

				/*
				float controlValues[12] = {
					pointPositions[0].x, pointPositions[0].y, pointPositions[0].z,
					pointPositions[1].x, pointPositions[1].y, pointPositions[1].z,
					tangentPositions[0].x, tangentPositions[0].y, tangentPositions[0].z,
					tangentPositions[1].x, tangentPositions[1].y, tangentPositions[1].z
				};*/

				float controlValues[12] = {
					pointPositions[0].x, pointPositions[1].x, tangentPositions[0].x, tangentPositions[1].x,
					pointPositions[0].y, pointPositions[1].y, tangentPositions[0].y, tangentPositions[1].y,
					pointPositions[0].z, pointPositions[1].z, tangentPositions[0].z, tangentPositions[1].z					
				};

				/*
				float controlValues[8] = {
					pointPositions[0].x, pointPositions[0].y,
					pointPositions[1].x, pointPositions[1].y,
					tangentPositions[0].x, tangentPositions[0].y,
					tangentPositions[1].x, tangentPositions[1].y
				};
				*/

				//glm::mat2x4 controlMatrix = glm::make_mat2x4(controlValues);
				glm::mat3x4 controlMatrix = glm::make_mat3x4(controlValues);

				
				/*
				glm::mat3x4 controlMatrix(pointPositions[0].x, pointPositions[0].y, pointPositions[0].z,
					pointPositions[1].x, pointPositions[1].y, pointPositions[1].z,
					tangentPositions[0].x, tangentPositions[0].y, tangentPositions[0].z,
					tangentPositions[1].x, tangentPositions[1].y, tangentPositions[1].z);
				*/

				//glm::mat4x3 controlMatrix;

				//glm::mat3x4 controlMatrix(pointPositions[0], pointPositions[1], pointPositions[0], pointPositions[0])

				//lines.push_back(glm::vec3(param*hermiteBasisMatrix*controlMatrix, 1));
				
				
				//lines.push_back(*myMat1x4Multiply((param*glm::transpose(hermiteBasisMatrix)),controlMatrix));
				
				glm::vec4 tmp = *anotherFunkyM(param, hermiteBasisMatrix);

				lines.push_back(*myMat1x4Multiply(tmp, controlMatrix));
				
				//lines.push_back(glm::transpose(controlMatrix*glm::transpose(hermiteBasisMatrix)*param));
				u += 0.005f;

				
			}

			u = 0;
		}

		delete g_vertex_buffer_data;

		int pointsBufferSize = (pointPositions.size() + tangentPositions.size()) * 3;
		int tangeantLinesBufferSize = tangentPositions.size() * 2 * 3;
		g_vertex_buffer_data = new GLfloat[pointsBufferSize + tangeantLinesBufferSize + (lines.size() * 3)];

		for (unsigned i = 0; i < pointPositions.size(); i++)
		{
			g_vertex_buffer_data[i * 3] = pointPositions[i].x;
			g_vertex_buffer_data[i * 3 + 1] = pointPositions[i].y;
			g_vertex_buffer_data[i * 3 + 2] = pointPositions[i].z;
		}

		int offset = pointPositions.size() * 3;

		for (unsigned i = 0; i < tangentPositions.size(); i++)
		{
			g_vertex_buffer_data[offset + i * 3] = tangentPositions[i].x;
			g_vertex_buffer_data[offset + i * 3 + 1] = tangentPositions[i].y;
			g_vertex_buffer_data[offset + i * 3 + 2] = tangentPositions[i].z;
		}

		
		for (unsigned i = 0; i < tangentPositions.size(); i++)
		{
			g_vertex_buffer_data[pointsBufferSize + i * 6] = pointPositions[i].x;
			g_vertex_buffer_data[pointsBufferSize + i * 6 + 1] = pointPositions[i].y;
			g_vertex_buffer_data[pointsBufferSize + i * 6 + 2] = pointPositions[i].z;
			g_vertex_buffer_data[pointsBufferSize + i * 6 + 3] = tangentPositions[i].x;
			g_vertex_buffer_data[pointsBufferSize + i * 6 + 4] = tangentPositions[i].y;
			g_vertex_buffer_data[pointsBufferSize + i * 6 + 5] = tangentPositions[i].z;
		}

		

		offset = pointsBufferSize + tangeantLinesBufferSize;

		for (unsigned i = 0; i < lines.size(); i++)
		{
			g_vertex_buffer_data[offset + i * 3] = lines[i].x;
			g_vertex_buffer_data[offset + i * 3 + 1] = lines[i].y;
			g_vertex_buffer_data[offset + i * 3 + 2] = 1;// lines[i].z;
		}

		glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data) * (pointsBufferSize + tangeantLinesBufferSize + (lines.size() * 3)), g_vertex_buffer_data, GL_DYNAMIC_DRAW);




		//Pass the values of the three matrices to the shaders
		glUniformMatrix4fv(proj_matrix_id, 1, GL_FALSE, glm::value_ptr(proj_matrix));
		glUniformMatrix4fv(view_matrix_id, 1, GL_FALSE, glm::value_ptr(view_matrix));
		glUniformMatrix4fv(model_matrix_id, 1, GL_FALSE, glm::value_ptr(model_matrix));

		glBindVertexArray(VAO);
		// Draw the triangle !
		glDrawArrays(GL_POINTS, 0, pointsBufferSize / 3); // Starting from vertex 0; 3 vertices total -> 1 triangle
		glDrawArrays(GL_LINES, pointsBufferSize / 3, tangeantLinesBufferSize / 3);
		glDrawArrays(GL_LINE_STRIP, pointsBufferSize / 3 + tangeantLinesBufferSize / 3, lines.size());


		glBindVertexArray(0);

		// update other events like input handling
		glfwPollEvents();
		// put the stuff we've been drawing onto the display
		glfwSwapBuffers(window);
	}

	cleanUp();
	return 0;
}