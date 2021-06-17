#version 330

uniform mat4 mM;  // Model matrix
uniform mat4 mV;  // View matrix
uniform mat4 mP;  // Projection matrix

layout (location = 0) in vec3 position;

out vec3 vNormal;    // vertex normal in world space
  
void main()
{
    gl_Position = mP * mV * mM * vec4(position, 1.0);
}
