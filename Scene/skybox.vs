#version 330

uniform mat4 mV;  // View matrix
uniform mat4 mP;  // Projection matrix

layout (location = 0) in vec3 vert_position;

out vec3 texCoords;

void main() {
    texCoords = vert_position;

    // remove the translation section of the camera view matrices
    mat4 viewT = mat4(mat3(mV)); 

    vec4 pos = mP * viewT * vec4(vert_position, 1.0);
    gl_Position = pos.xyww;
}  