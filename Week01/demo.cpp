#define GLFW_INCLUDE_ES2 1
#define GLFW_DLL 1
//#define GLFW_EXPOSE_NATIVE_WIN32 1
//#define GLFW_EXPOSE_NATIVE_EGL 1

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <GLFW/glfw3.h>
//#include <GLFW/glfw3native.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <fstream> 
#include "bitmap.h"

#include <fmod.hpp>
#include <fmod_errors.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define TEXTURE_COUNT 2

//Must be power of 2
#define SPECTRUM_SIZE 1024

GLint GprogramID = -1;
GLuint GtextureID[TEXTURE_COUNT];

GLFWwindow* window;

float m_spectrumLeft[SPECTRUM_SIZE];
float m_spectrumRight[SPECTRUM_SIZE];

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

//FMOD Error Check
void ERRCHECK(FMOD_RESULT result)
{
	if (result != FMOD_OK)
	{
		printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
	}
}

FMOD::System* m_fmodSystem;
FMOD::Sound* m_music;
FMOD::Channel* m_musicChannel;

GLuint LoadShader(GLenum type, const char *shaderSrc)
{
	GLuint shader;
	GLint compiled;

	// Create the shader object
	shader = glCreateShader(type);

	if (shader == 0)
		return 0;

	// Load the shader source
	glShaderSource(shader, 1, &shaderSrc, NULL);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled)
	{
		GLint infoLen = 0;

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1)
		{
			char infoLog[4096];
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			printf("Error compiling shader:\n%s\n", infoLog);
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint LoadShaderFromFile(GLenum shaderType, std::string path)
{
	GLuint shaderID = 0;
	std::string shaderString;
	std::ifstream sourceFile(path.c_str());

	if (sourceFile)
	{
		shaderString.assign((std::istreambuf_iterator< char >(sourceFile)), std::istreambuf_iterator< char >());
		const GLchar* shaderSource = shaderString.c_str();

		return LoadShader(shaderType, shaderSource);
	}
	else
		printf("Unable to open file %s\n", path.c_str());

	return shaderID;
}

void loadTexture(const char* path, GLuint textureID)
{
	CBitmap bitmap(path);

	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// bilinear filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.GetWidth(), bitmap.GetHeight(), 0,
		GL_RGBA, GL_UNSIGNED_BYTE, bitmap.GetBits());
}

void initFmod()
{
	FMOD_RESULT result;
	unsigned int version;

	result = FMOD::System_Create(&m_fmodSystem);
	ERRCHECK(result);

	result = m_fmodSystem->getVersion(&version);
	ERRCHECK(result);

	if (version < FMOD_VERSION)
	{
		printf("FMOD Error! You are using an old version of FMOD.", version, FMOD_VERSION);
	}

	//Initialise fmod system
	result = m_fmodSystem->init(32, FMOD_INIT_NORMAL, 0);
	ERRCHECK(result);

	//Load and Set up Music
	result = m_fmodSystem->createStream("../media/Song4.mp3", FMOD_SOFTWARE, 0, &m_music);
	ERRCHECK(result);

	//Play the loaded mp3 music
	result = m_fmodSystem->playSound(FMOD_CHANNEL_FREE, m_music, false, &m_musicChannel);
	ERRCHECK(result);
}

void updateFmod()
{
	m_fmodSystem->update();

	//Get spectrum for left and right stereo channels
	m_musicChannel->getSpectrum(m_spectrumLeft, SPECTRUM_SIZE, 0, FMOD_DSP_FFT_WINDOW_RECT);
	m_musicChannel->getSpectrum(m_spectrumRight, SPECTRUM_SIZE, 0, FMOD_DSP_FFT_WINDOW_RECT);

	//Print the first audio spectrum for both left and right channels
	std::cout << m_spectrumLeft[0] << ", " << m_spectrumRight[0] << std::endl;
}


int Init(void)
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint programObject;
	GLint linked;

	//load textures
	glGenTextures(TEXTURE_COUNT, GtextureID);
	loadTexture("../media/rainbowblocks.bmp", GtextureID[0]);
	//loadTexture("../media/background.bmp", GtextureID[1]);

	vertexShader = LoadShaderFromFile(GL_VERTEX_SHADER, "../vertexShader1.vert");
	fragmentShader = LoadShaderFromFile(GL_FRAGMENT_SHADER, "../fragmentShader9.frag");

	// Create the program object
	programObject = glCreateProgram();

	if (programObject == 0)
		return 0;

	glAttachShader(programObject, fragmentShader);
	glAttachShader(programObject, vertexShader);


	// Bind vPosition to attribute 0   
	glBindAttribLocation(programObject, 0, "vPosition");
	glBindAttribLocation(programObject, 1, "VColor");
	glBindAttribLocation(programObject, 2, "vTexCoord");

	// Link the program
	glLinkProgram(programObject);

	// Check the link status
	glGetProgramiv(programObject, GL_LINK_STATUS, &linked);

	if (!linked)
	{
		GLint infoLen = 0;

		glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1)
		{
			//char* infoLog = malloc (sizeof(char) * infoLen );
			char infoLog[512];
			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
			printf("Error linking program:\n%s\n", infoLog);

			//free ( infoLog );
		}

		glDeleteProgram(programObject);
		return 0;
	}

	// Store the program object
	GprogramID = programObject;

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	return 1;
}

void Draw(void)
{
	//set the sampler2D varying variable to the first texture unit(int)
	glUniform1i(glGetUniformLocation(GprogramID, "sampler2d"), 0);

	//modify factor1 varying variable
	static float factor1 = 0.0f;
	factor1 += 0.01f;
	GLint factor1Loc = glGetUniformLocation(GprogramID, "Factor1");
	if (factor1Loc != -1)
	{
		glUniform1f(factor1Loc, factor1);
	}

	//modify factor1 varying variable
	static float factor2 = 0.0f;
	factor2 += 0.05f;
	GLint factor2Loc = glGetUniformLocation(GprogramID, "Factor2");
	if (factor2Loc != -1)
	{
		glUniform1f(factor2Loc, factor2);
	}

	static float factor3 = 0.0f;
	factor3 += 0.08f;
	GLint factor3Loc = glGetUniformLocation(GprogramID, "Factor3");
	if (factor3Loc != -1)
	{
		glUniform1f(factor3Loc, factor3);
	}

	//Fmod Update
	updateFmod();
	GLint spectrum1Loc = glGetUniformLocation(GprogramID, "Spectrum1");
	if (spectrum1Loc != 1)
	{
		glUniform1f(spectrum1Loc, m_spectrumRight[0]);
	}

	GLint spectrum2Loc = glGetUniformLocation(GprogramID, "Spectrum2");
	if (spectrum2Loc != 1)
	{
		glUniform1f(spectrum2Loc, m_spectrumLeft[0]);
	}

	GLint spectrum3Loc = glGetUniformLocation(GprogramID, "Spectrum3");
	if (spectrum3Loc != 1)
	{
		glUniform1f(spectrum3Loc, m_spectrumLeft[0] + m_spectrumRight[0]);
	}

	/*GLfloat vVertices[] = {-0.5f,  0.5f, 0.0f,
	-0.5f, -0.5f, 0.0f,
	0.5f, -0.5f,  0.0f,
	0.5f, -0.5f, 0.0f,
	0.5f, 0.5f, 0.0f,
	-0.5f, 0.5f, 0.0f};*/

	static GLfloat vVertices[] = { -1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f };

	GLfloat vColors[] = { 1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f };

	static GLfloat vTexCoords[] = { 0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f };
	/*
	GLfloat vVertices[] = {0.0f,  1.0f, 0.0f,
	-1.0f, -1.0f, 0.0f,
	1.0f, -1.0f,  0.0f};*/

	// Set the viewport
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	// Clear the color buffer
	glClear(GL_COLOR_BUFFER_BIT);

	// Use the program object
	glUseProgram(GprogramID);

	// Load the vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, vColors);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, vTexCoords);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glDrawArrays(GL_TRIANGLES, 0, 6);

}

int main(void)
{
	glfwSetErrorCallback(error_callback);

	// Initialize GLFW library
	if (!glfwInit())
		return -1;

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	// Create and open a window
	window = glfwCreateWindow(WINDOW_WIDTH,
		WINDOW_HEIGHT,
		"Hello World",
		NULL,
		NULL);

	if (!window)
	{
		glfwTerminate();
		printf("glfwCreateWindow Error\n");
		exit(1);
	}

	glfwMakeContextCurrent(window);

	Init();
	initFmod();

	// Repeat
	while (!glfwWindowShouldClose(window)) {


		Draw();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
