#version 330

out vec4 fragColor;

in vec3 texCoords;

// skybox texture
uniform samplerCube skybox;

void main()
{    
	fragColor = texture(skybox, texCoords);
}