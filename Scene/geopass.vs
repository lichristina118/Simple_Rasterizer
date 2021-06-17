#version 330

uniform mat4 mM;  // Model matrix
uniform mat4 mV;  // View matrix
uniform mat4 mP;  // Projection matrix

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

out vec3 vNormal;    // vertex normal in world space

void main()
{
    vNormal = (transpose(inverse(mM)) * vec4(normal, 0.0)).xyz;
    vNormal = normalize(vNormal);
    gl_Position = mP * mV * mM * vec4(position, 1.0);
}
