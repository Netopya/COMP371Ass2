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
GLuint is_triangle_id = 0;

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

vector<glm::mat3x4> controlMatrices;

GLfloat* g_vertex_buffer_data;

GLfloat triangleBuffer[9] = { 0,0,1,-0.05,-0.1,1,0.05,-0.1,1 };
const int TRIANGLE_BUFFER_SIZE = 9;

// enums to hold keyboard and mouse input
enum ArrowKeys { none, left, right, up, down };
ArrowKeys arrowKey;

enum ViewMode { points, splines };
ViewMode viewMode = ViewMode::splines;

bool enterPressed = false;
bool splinesComputed = false;
bool backspacePressed = false;

// Helper function to convert vec3's into a formatted string
string vec3tostring(glm::vec3 vec)
{
	std::stringstream ss;
	ss << vec.x << ' ' << vec.y << ' ' << vec.z;
	return ss.str();
}

///Handle the keyboard input
void keyPressed(GLFWwindow *_window, int key, int scancode, int action, int mods) {
	if (action == GLFW_RELEASE)
	{
		arrowKey = ArrowKeys::none;
	}
	else if (action == GLFW_PRESS)
	{
		switch (key) {
		case GLFW_KEY_LEFT:
			arrowKey = ArrowKeys::left;
			break;
		case GLFW_KEY_RIGHT:
			arrowKey = ArrowKeys::right;
			break;
		case GLFW_KEY_UP:
			arrowKey = ArrowKeys::up;
			break;
		case GLFW_KEY_DOWN:
			arrowKey = ArrowKeys::down;
			break;
		case GLFW_KEY_P:
			viewMode = ViewMode::points;
			break;
		case GLFW_KEY_L:
			viewMode = ViewMode::splines;
			break;
		case GLFW_KEY_ENTER:
			if (!splinesComputed)
			{
				enterPressed = true;
			}
			break;
		case GLFW_KEY_BACKSPACE:
			backspacePressed = true;
			break;
		default: break;
		}
	}

	return;
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
	//mousePosition *= glm::inverse(view_matrix);
	mousePosition.y = -mousePosition.y;

	// Offset by camera position
	mousePosition.x -= view_matrix[3].x;
	mousePosition.y -= view_matrix[3].y;

	//cout << vec3tostring(mousePosition) << endl;
}

// Handle the window resizing, set the viewport and recompute the perspective
void windowResized(GLFWwindow *windows, int width, int height)
{
	glViewport(0, 0, width, height);
	float difference = (float)width * 2.0f / (float)height;
	difference /= 2.0f;

	proj_matrix = glm::ortho(1.0f - difference, 1.0f + difference, -1.0f, 1.0f);
}

glm::vec3* calculateSplinePoint(float u, glm::mat3x4 &controlMatrix)
{
	glm::vec4 param(u*u*u, u*u, u, 1);
	return new glm::vec3(param*glm::transpose(hermiteBasisMatrix)*controlMatrix);
}

// Based on https://www.youtube.com/watch?v=T143lv8aKDU
float calculateCurvature(float u, glm::mat3x4 &controlMatrix)
{
	glm::vec3 velocity(glm::vec4(3 * u*u, 2 * u, 1, 0)*glm::transpose(hermiteBasisMatrix)*controlMatrix);
	glm::vec3 acceleration(glm::vec4(6 * u, 2, 0, 0)*glm::transpose(hermiteBasisMatrix)*controlMatrix);

	//return glm::length(glm::cross(velocity, acceleration)) / pow(glm::length(velocity), 3);

	float mv = glm::length(velocity);
	return glm::length(glm::cross(velocity, acceleration)) / (mv * mv * mv);
}

void subDivide(float u1, float u2, glm::mat3x4 &controlMatrix, bool addFirstPoint)
{
	float umid = (u1 + u2) / 2.0f;
	glm::vec3* point1 = calculateSplinePoint(u1, controlMatrix);
	glm::vec3* point2 = calculateSplinePoint(u2, controlMatrix);

	if (addFirstPoint)
	{
		lines.push_back(*point1);
	}

	float curvature = calculateCurvature((u1 + umid) / 2, controlMatrix);
	//cout << "at:" << umid << " between " << u1 << " and " << u2 << " curve is: " << curvature << " flip? " << addFirstPoint << endl;
	//if (glm::length(glm::distance(*point1, *point2)) > 0.01f)
	if(curvature > 1.0f && glm::length(glm::distance(*point1, *point2)) > 0.05f)
	{
		subDivide(u1, umid, controlMatrix, false);
		subDivide(umid, u2, controlMatrix, true);
	}

	lines.push_back(*point2);
}

void calculateSplines()
{
	lines.clear();

	for (unsigned i = 0; i < tangentPositions.size() - 1; i++)
	{
		float controlValues[12] = {
			pointPositions[i].x, pointPositions[i + 1].x, tangentPositions[i].x, tangentPositions[i + 1].x,
			pointPositions[i].y, pointPositions[i + 1].y, tangentPositions[i].y, tangentPositions[i + 1].y,
			pointPositions[i].z, pointPositions[i + 1].z, tangentPositions[i].z, tangentPositions[i + 1].z
		};

		glm::mat3x4 controlMatrix = glm::make_mat3x4(controlValues);

		controlMatrices.push_back(controlMatrix);

		//glm::vec3* point1 = calculateSplinePoint(0, controlMatrix);
		//lines.push_back(*point1);

		subDivide(0.0f, 0.5f, controlMatrix, true);
		subDivide(0.5f, 1.0f, controlMatrix, true);

	}
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
	glfwSetWindowSizeCallback(window, windowResized);

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

	lines.clear();
	pointPositions.clear();
	tangentPositions.clear();
	controlMatrices.clear();

	enterPressed = false;
	splinesComputed = false;
	backspacePressed = false;

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
	is_triangle_id = glGetUniformLocation(ProgramID, "is_triangle");

	return ProgramID;
}


int main() {

	while (true)
	{



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

		/*
		pointPositions.push_back(glm::vec3(0, 0, 1));
		pointPositions.push_back(glm::vec3(0.5, 0, 1));
		tangentPositions.push_back(glm::vec3(0, 0.5, 1));
		tangentPositions.push_back(glm::vec3(0.5, 0.5, 1));
		*/

		float u = 0;
		int controlIndex = 0;

		while (!glfwWindowShouldClose(window) && !backspacePressed) {
			// wipe the drawing surface clear
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glClearColor(0.1f, 0.2f, 0.2f, 1.0f);
			glPointSize(point_size);

			glUseProgram(shader_program);

			//g_vertex_buffer_data[0] = mousePosition.x;
			//g_vertex_buffer_data[1] = mousePosition.y;

			//model_matrix = glm::translate(oriModel, mousePosition);

			// 
			switch (arrowKey)
			{
			case ArrowKeys::none:
				break;
			case ArrowKeys::left:
				view_matrix = glm::translate(view_matrix, glm::vec3(-0.01f, 0.0f, 0.0f));
				break;
			case ArrowKeys::right:
				view_matrix = glm::translate(view_matrix, glm::vec3(0.01f, 0.0f, 0.0f));
				break;
			case ArrowKeys::up:
				view_matrix = glm::translate(view_matrix, glm::vec3(0.0f, 0.01f, 0.0f));
				break;
			case ArrowKeys::down:
				view_matrix = glm::translate(view_matrix, glm::vec3(0.0f, -0.01f, 0.0f));
				break;
			default:
				break;
			}

			if (tangentPositions.size() > 1 && tangentPositions.size() == pointPositions.size() && enterPressed)
			{
				enterPressed = false;

				calculateSplines();

				splinesComputed = true;
			}

			delete g_vertex_buffer_data;

			int pointsBufferSize = (pointPositions.size() + tangentPositions.size()) * 3;
			int tangeantLinesBufferSize = tangentPositions.size() * 2 * 3;
			g_vertex_buffer_data = new GLfloat[pointsBufferSize + tangeantLinesBufferSize + (lines.size() * 3) + TRIANGLE_BUFFER_SIZE];

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

			offset += lines.size() * 3;

			for (int i = 0; i < TRIANGLE_BUFFER_SIZE; i++)
			{
				g_vertex_buffer_data[offset + i] = triangleBuffer[i];
			}


			glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data) * (pointsBufferSize + tangeantLinesBufferSize + (lines.size() * 3) + TRIANGLE_BUFFER_SIZE), g_vertex_buffer_data, GL_DYNAMIC_DRAW);




			//Pass the values of the three matrices to the shaders
			glUniformMatrix4fv(proj_matrix_id, 1, GL_FALSE, glm::value_ptr(proj_matrix));
			glUniformMatrix4fv(view_matrix_id, 1, GL_FALSE, glm::value_ptr(view_matrix));
			glUniformMatrix4fv(model_matrix_id, 1, GL_FALSE, glm::value_ptr(model_matrix));
			glUniform1i(is_triangle_id, 0);

			glBindVertexArray(VAO);
			// Draw the triangle !
			glDrawArrays(GL_POINTS, 0, pointsBufferSize / 3); // Starting from vertex 0; 3 vertices total -> 1 triangle
			glDrawArrays(GL_LINES, pointsBufferSize / 3, tangeantLinesBufferSize / 3);

			if (viewMode == ViewMode::splines)
			{
				glDrawArrays(GL_LINE_STRIP, pointsBufferSize / 3 + tangeantLinesBufferSize / 3, lines.size());
			}
			else {
				glDrawArrays(GL_POINTS, pointsBufferSize / 3 + tangeantLinesBufferSize / 3, lines.size());
			}




			if (controlMatrices.size() > 0)
			{
				glm::vec3 trianglePosition = glm::vec4(u*u*u, u*u, u, 1)*glm::transpose(hermiteBasisMatrix)*controlMatrices[controlIndex];
				trianglePosition.z = 0;
				glm::vec3 triangleAngle = glm::vec4(3 * u*u, 2 * u, 1, 0)*glm::transpose(hermiteBasisMatrix)*controlMatrices[controlIndex];
				triangleAngle.z = 0;
				float hyp = sqrt(triangleAngle[0] * triangleAngle[0] + triangleAngle[1] * triangleAngle[1]);
				float msin = triangleAngle[1] / hyp;
				float cos = triangleAngle[0] / hyp;
				glm::mat2 triangleRotation(msin, -1.0f * cos, cos, msin);
				//glm::mat2 triangleRotation(cos, -1.0f * msin, msin, cos);
				glm::mat4 triangleModel2(model_matrix * glm::mat4(triangleRotation));
				triangleModel2[2].z = 1;
				triangleModel2[3].w = 1;
				glm::mat4 triangleModel = glm::translate(model_matrix, trianglePosition);
				//triangleModel = glm::rotate(triangleModel, atan2(triangleAngle[1], triangleAngle[0]), glm::vec3(0.0f, 0.0f, 1.0f));

				//float curvature = calculateCurvature(u, controlMatrices[controlIndex]);

				//cout << curvature << endl;

				//triangleModel = glm::scale(triangleModel, glm::vec3(1, curvature, 1));
				triangleModel = triangleModel * triangleModel2;


				glUniform1i(is_triangle_id, 1);
				glUniformMatrix4fv(model_matrix_id, 1, GL_FALSE, glm::value_ptr(triangleModel));

				glDrawArrays(GL_TRIANGLES, pointsBufferSize / 3 + tangeantLinesBufferSize / 3 + lines.size(), 3);

				u += 0.005;

				if (u > 1)
				{
					u = 0;
					controlIndex++;
					if (controlIndex == controlMatrices.size())
					{
						controlIndex = 0;
					}
				}
			}


			glBindVertexArray(0);

			// update other events like input handling
			glfwPollEvents();
			// put the stuff we've been drawing onto the display
			glfwSwapBuffers(window);
		}

		cleanUp();
	}
	return 0;
}