#version 330

out vec4 pixelColor; //Output variable. Almost final pixel color.
uniform sampler2D textureMap;

in vec2 texCrd;

void main(void) {
	vec4 tx = texture(textureMap, texCrd);
	pixelColor=tx;
}
