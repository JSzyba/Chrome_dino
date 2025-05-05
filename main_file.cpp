/*
"Chrome Dino" (https://skfb.ly/ouZJN) by Shadow Models 3D is licensed under Creative Commons Attribution (http://creativecommons.org/licenses/by/4.0/).

*/



#include <iostream>
#include <glew.h>
#include <glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <string>
#include "lodepng.h"
#include "GLM\glm.hpp"
#include "GLM\gtc\type_ptr.hpp"
#include "GLM\gtc\matrix_transform.hpp"
#include "shaderprogram.h"
#include "constants.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <ft2build.h>
#include FT_FREETYPE_H

float aspectRatio;	// width to height ratio of window
float x = 0.0f, y = 0.0f, z = 0.0f;

ShaderProgram* sp;	// pointer to the shader program
ShaderProgram* sptx;

double prevTime, currTime;
double speedX=10, speedY=0;

unsigned int score = 0;
float scoreFraction = 0.0f;
double cooldown = 3;
bool hit = false;

// struct containing all attributes of vertices
struct Vertex {
	std::vector<glm::vec4> position;
	std::vector<glm::vec4> normal;
	std::vector<glm::vec2> texCoords;
};

// struct containg all important information about texture
struct Texture {
	aiString type;
	GLuint tex;
};

// struct containing all important information about mesh
struct Mesh {
	Vertex vertex;
	std::vector<unsigned int> indices;
	Texture tex;
};

struct Character {
	unsigned int TextureID; // ID handle of the glyph texture
	glm::ivec2   Size;      // Size of glyph
	glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
	unsigned int Advance;   // Horizontal offset to advance to next glyph
};

std::map<GLchar, Character> Characters;

Mesh dino, enemy;
std::vector<Texture> texVec;
glm::vec3 enemyArray[64];
int enemyCount = 0;

bool jump = false;
bool canSpawn = false;


void RenderText(std::string text, float x, float y, float scale, glm::vec3 color)
{
	// activate corresponding render state    
	sptx->use();
	glUniform3f(sptx->u("textColor"), color.x, color.y, color.z);
	glActiveTexture(GL_TEXTURE0);

	// Define indices for the quad (2 triangles forming a rectangle)
	GLuint indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	// iterate through all characters
	std::string::const_iterator c;
	for (c = text.begin(); c != text.end(); c++)
	{
		Character ch = Characters[*c];

		float xpos = x + ch.Bearing.x * scale;
		float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

		float w = ch.Size.x * scale;
		float h = ch.Size.y * scale;

		// Define the vertices for the current character
		float vertices[4][4] = {
			{ xpos,     ypos + h,   0.0f, 0.0f },
			{ xpos,     ypos,       0.0f, 1.0f },
			{ xpos + w, ypos,       1.0f, 1.0f },
			{ xpos + w, ypos + h,   1.0f, 0.0f }
		};

		// Bind the texture of the current character
		glBindTexture(GL_TEXTURE_2D, ch.TextureID);

		// Enable and specify vertex attributes (position and texture coordinates)
		glEnableVertexAttribArray(10);
		glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices);

		// Draw the character using glDrawElements
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, indices);

		// Disable the vertex attribute array
		glDisableVertexAttribArray(10);

		// Now advance cursors for next glyph (note that advance is number of 1/64 pixels)
		x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64)
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint readTexture(const char* filename) {
	GLuint tex;
	glActiveTexture(GL_TEXTURE0);

	//Load into computer's memory
	std::vector<unsigned char> image;   //Allocate a vector for image storage
	unsigned width, height;   //Variables for image size
	//Read image
	unsigned error = lodepng::decode(image, width, height, filename);
	//Import into graphics card's memory
	glGenTextures(1, &tex); //Initialize one handle
	glBindTexture(GL_TEXTURE_2D, tex); //Activate the handle
	//Import image into graphics card's memory associated with the handle
	glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, (unsigned char*)image.data());

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	printf(filename);
	printf(": %d\n", tex);
	return tex;
}

// procedures and functions used to read model from file using assimp library
void processMesh(aiMesh* mesh, const aiScene* scene, Mesh* givenMesh) {
	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		givenMesh->vertex.position.push_back(glm::vec4(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1));
		givenMesh->vertex.normal.push_back(glm::vec4(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z, 0));
		if (mesh->mTextureCoords[0]) {
			givenMesh->vertex.texCoords.push_back(glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y));
		}
		else {
			givenMesh->vertex.texCoords.push_back(glm::vec2(0.0f, 0.0f));
		}

	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++) {
			givenMesh->indices.push_back(face.mIndices[j]);
		}
	}

	aiString textureName;	// name of the texture
	scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), textureName);	// getting first diffuse texture name
	// searching for texture in texVec
	for (int i = 0; i < texVec.size(); i++) {
		if (texVec[i].type == textureName) {
			givenMesh->tex = texVec[i];
			break;
		}
	}
}
void processNode(aiNode* node, const aiScene* scene, Mesh& meshes) {
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		processMesh(mesh, scene, &meshes);
		//if (meshes.indices.size() > 0)	meshes.push_back(givenMesh);
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene, meshes);
	}
}
//YOU'RE SINGLE
bool loadModel(const std::string& path, Mesh& mesh) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		return false;
	}

	if (scene->HasMaterials())	// true when number of materials is greater than 0
	{
		for (int i = 0; i < scene->mNumMaterials; i++)
		{
			aiReturn ret;	// success flag

			aiMaterial* material = scene->mMaterials[i];	// get the current material

			aiString materialName;	// name of the material
			ret = material->Get(AI_MATKEY_NAME, materialName);	// get the material name
			if (ret != AI_SUCCESS) materialName = "";			// check if material was found

			int numTextures = material->GetTextureCount(aiTextureType_DIFFUSE); // amount of diffuse textures

			aiString textureName;	// filename of the texture
			if (numTextures > 0) {	// check if there are any diffuse textures
				material->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), textureName);	// getting first diffuse texture

				// Creating new Texture 
				Texture newTex;
				newTex.type = textureName;
				newTex.tex = readTexture(textureName.C_Str());
				texVec.push_back(newTex);
			}
		}
	}

	processNode(scene->mRootNode, scene, mesh);
	return true;
}

void error_callback(int error, const char* description) {
	fputs(description, stderr);
}

void windowResizeCallback(GLFWwindow* window, int width, int height) {
	if (height == 0) return;
	aspectRatio = (float)width / (float)height;
	glViewport(0, 0, width, height);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_SPACE && !jump)
		{
			jump = true;
			speedY = 18;
		}
		if (key == GLFW_KEY_T)
			printf("curr time: %f\n", glfwGetTime());
		if (key == GLFW_KEY_S)
			printf("score: %d\n", score);
		if (key == GLFW_KEY_V)
		{
			float highest = -1000.0f, lowest = 1000.0f;
			int highest_i = 0.0f, lowest_i = 0.0f;
			for (int i = 0; i < dino.vertex.position.size(); i++)
			{
				if (dino.vertex.position[i].y < lowest)
				{
					lowest = dino.vertex.position[i].y;
					lowest_i = i;
				}
				if (dino.vertex.position[i].y > highest)
				{
					highest = dino.vertex.position[i].y;
					highest_i = i;
				}
			}
			printf("highest: %f\nlowest: %f\n", highest, lowest);
			//highest: 432
			//lowest: 69
			//avg: 2.478605
		}
		if (key == GLFW_KEY_B)
			printf("avg: %f\n", (float)((dino.vertex.position[432].y-dino.vertex.position[69].y)/2.0f));
		if(key == GLFW_KEY_H)
			printf(": %d\n", dino.tex.tex);
		if (key == GLFW_KEY_I)
			printf(": %d\n", enemy.tex.tex);
	}
}

void initOpenGLProgram(GLFWwindow* window) {
	aspectRatio = 1;
	srand(time(NULL));
	glClearColor(0.67f, 0.84f, 0.9f, 1);
	glEnable(GL_DEPTH_TEST);
	glfwSetWindowSizeCallback(window, windowResizeCallback);
	glfwSetKeyCallback(window, keyCallback);
	sp = new ShaderProgram("v_simplest.glsl", NULL, "f_simplest.glsl");
	sptx = new ShaderProgram("v_text.glsl", NULL, "f_text.glsl");
	if (!loadModel("rex.obj", dino) || !loadModel("rex2.obj", enemy))
	{
		fprintf(stderr,"Failed to load model!\n");
		exit(EXIT_FAILURE);
	}
}

void drawScene(GLFWwindow* window)
{
	if (cooldown <= 0)
		canSpawn = true;
	else
		cooldown -= double(currTime - prevTime);
	if (canSpawn && rand() / (double)RAND_MAX <= 0.004 && enemyCount < 64)
	{
		enemyArray[enemyCount] = glm::vec3(80.0f, 0.0f, 0.0f);
		enemyCount++;
		cooldown = 2;
		canSpawn = false;
		printf("Enemy added!\n");
	}
	if (jump)
	{
		y += speedY * double(currTime - prevTime);
		if (y < 0)
		{
			speedY = 0;
			y = 0;
			jump = false;
		}
		else
			speedY -= 30 * double(currTime - prevTime);
	}
	scoreFraction += currTime - prevTime;
	if (scoreFraction >= 0.5f)
	{
		score += floor(scoreFraction*2.0f);
		scoreFraction = 0.0f;
	}
	if (enemyCount && !hit)
	{
		if (abs(y - enemyArray[0].y) <= 5 && abs(enemyArray[0].x) <= 2.47)
		{
			hit = true;
			printf("Touch!\n");
		}
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glm::mat4 V = glm::lookAt(
		glm::vec3(-6.0f, 0.0f, 4.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f)); //compute view matrix
	glm::mat4 P = glm::perspective(50.0f * PI / 180.0f, aspectRatio, 1.0f, 500.0f); //compute projection matrix
	glm::mat4 M = glm::mat4(1.0f);
	M = glm::translate(M, glm::vec3(x,y,z));
	
	sp->use();	//activate shading program
	glUniformMatrix4fv(sp->u("P"), 1, false, glm::value_ptr(P));
	glUniformMatrix4fv(sp->u("V"), 1, false, glm::value_ptr(V));
	glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M));
	glUniform1i(sp->u("textureMap"), 0);

	glBindTexture(GL_TEXTURE_2D, dino.tex.tex);

	glEnableVertexAttribArray(sp->a("vertex"));		// enable sending data to the attribute vertex
	glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, dino.vertex.position.data());		// specify source of the data for the attribute vertex

	glEnableVertexAttribArray(sp->a("texCoord"));		// enable sending data to the attribute vertex
	glVertexAttribPointer(sp->a("texCoord"), 2, GL_FLOAT, false, 0, dino.vertex.texCoords.data());		// specify source of the data for the attribute vertex

	glDrawElements(GL_TRIANGLES, dino.indices.size(), GL_UNSIGNED_INT, dino.indices.data());	// draw mesh

	for (int i = 0; i < enemyCount; i++)
	{
		glm::mat4 Me = glm::mat4(1.0f);
		Me = glm::translate(Me, enemyArray[i]);
		glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(Me));

		glBindTexture(GL_TEXTURE_2D, enemy.tex.tex);

		glEnableVertexAttribArray(sp->a("vertex"));		// enable sending data to the attribute vertex
		glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, enemy.vertex.position.data());		// specify source of the data for the attribute vertex

		glEnableVertexAttribArray(sp->a("texCoord"));		// enable sending data to the attribute vertex
		glVertexAttribPointer(sp->a("texCoord"), 2, GL_FLOAT, false, 0, enemy.vertex.texCoords.data());		// specify source of the data for the attribute vertex

		glDrawElements(GL_TRIANGLES, enemy.indices.size(), GL_UNSIGNED_INT, enemy.indices.data());	// draw mesh
		enemyArray[i].x -= 10*double(currTime - prevTime);
		if (enemyArray[i].x < -10.0f)
		{
			for (int j = i; j < enemyCount - 1; j++)
				enemyArray[j] = enemyArray[j+1];
			enemyCount--;
		}
	}
	glDisableVertexAttribArray(sp->a("vertex")); //Disable sending data to the attribute vertex
	glDisableVertexAttribArray(sp->a("texCoord")); //Disable sending data to the attribute vertex

	sptx->use();

	RenderText("Score: " + std::to_string(score), 25.0f, 25.0f, 1.0f, glm::vec3(0.5, 0.8f, 0.2f));

	glfwSwapBuffers(window);
}

void freeOpenGLProgram(GLFWwindow* window) {
	glDeleteTextures(1, &dino.tex.tex);
	glDeleteTextures(1, &enemy.tex.tex);
	delete sp;
	delete sptx;
}

int main()
{
	
	GLFWwindow* window; //Pointer to object that represents the application window

	glfwSetErrorCallback(error_callback);//Register error processing callback procedure

	if (!glfwInit()) { //Initialize GLFW library
		fprintf(stderr, "Can't initialize GLFW.\n");
		exit(EXIT_FAILURE);
	}

	window = glfwCreateWindow(800, 600, "Dino", NULL, NULL);  //Create a window 800pxx600px titled "OpenGL" and an OpenGL context associated with it.

	if (!window) //If no window is opened then close the program
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window); //Since this moment OpenGL context corresponding to the window is active and all OpenGL calls will refer to this context.
	glfwSwapInterval(1); //During vsync wait for the first refresh

	GLenum err;
	if ((err = glewInit()) != GLEW_OK) { //Initialize GLEW library
		fprintf(stderr, "Can't initialize GLEW: %s\n", glewGetErrorString(err));
		exit(EXIT_FAILURE);
	}

	initOpenGLProgram(window); //Call initialization procedure

	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(800), 0.0f, static_cast<float>(600));
	sptx->use();
	glUniformMatrix4fv(sptx->u("projection"), 1, GL_FALSE, glm::value_ptr(projection));

	FT_Library ft;
	// All functions return a value different than 0 whenever an error occurred
	if (FT_Init_FreeType(&ft))
	{
		printf("ERROR::FREETYPE: Could not init FreeType Library\n");
		exit(EXIT_FAILURE);
	}

	FT_Face face;
	if (FT_New_Face(ft, "Roboto-Regular.ttf", 0, &face)) {
		printf("ERROR::FREETYPE: Failed to load font\n");
		exit(EXIT_FAILURE);
    }
	else {
		// set size to load glyphs as
		FT_Set_Pixel_Sizes(face, 0, 48);

		// disable byte-alignment restriction
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		// load first 128 characters of ASCII set
		for (unsigned char c = 0; c < 128; c++)
		{
			// Load character glyph 
			if (FT_Load_Char(face, c, FT_LOAD_RENDER))
			{
				std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
				continue;
			}
			// generate texture
			GLuint texture;
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RED,
				face->glyph->bitmap.width,
				face->glyph->bitmap.rows,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				face->glyph->bitmap.buffer
			);
			// set texture options
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			// now store character for later use
			Character character = {
				texture,
				glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
				glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
				static_cast<unsigned int>(face->glyph->advance.x)
			};
			Characters.insert(std::pair<char, Character>(c, character));
		}
	}
	FT_Done_Face(face);
	FT_Done_FreeType(ft);


	glfwSetTime(0); //Zero the timer
	//Main application loop
	prevTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) //As long as the window shouldnt be closed yet...
	{
		currTime = glfwGetTime();
		drawScene(window);
		glfwPollEvents();
		prevTime = currTime;
	}
	freeOpenGLProgram(window);

	glfwDestroyWindow(window); //Delete OpenGL context and the window.
	glfwTerminate(); //Free GLFW resources
	return 0;
}