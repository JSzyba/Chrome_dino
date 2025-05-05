#version 330

//Uniform variables
uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

//Attributes
in vec4 vertex; //Vertex coordinates in model space
in vec2 texCoord;

out vec2 texCrd;

void main(void) {
    texCrd = texCoord;
    gl_Position=P*V*M*vertex;
}
