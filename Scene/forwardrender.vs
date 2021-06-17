#version 330

uniform mat4 mM;  // Model matrix
uniform mat4 mV;  // View matrix
uniform mat4 mP;  // Projection matrix

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in ivec4 boneIDs;
layout (location = 3) in vec4 boneWts;

// 0 if no animation; 1 if has animation with bone info; 2 if has animation no bone info
uniform int  hasAnimation;
const int maxTotalBones = 100;
const int maxVertexBones = 4;
uniform mat4 boneTransform[maxTotalBones];

out vec3 vPosition;  // vertex position in world space
out vec3 vNormal;    // vertex normal in world space
//out vec4 temp;

void main()
{
    if (hasAnimation == 1) {
        mat4 bT = boneTransform[boneIDs[0]] * boneWts[0]
                    + boneTransform[boneIDs[1]] * boneWts[1]
                    + boneTransform[boneIDs[2]] * boneWts[2]
                    + boneTransform[boneIDs[3]] * boneWts[3];
        vec4 localPos = bT * vec4(position, 1.0);
        vec4 localNorm = bT * vec4(normal, 0.0);
        vPosition = (mM * localPos).xyz;
        vNormal = (transpose(inverse(mM)) * localNorm).xyz;
        vNormal = normalize(vNormal);

        //vPosition = (mM * vec4(position, 1.0)).xyz;
        //vNormal = (transpose(inverse(mM)) * vec4(normal, 0.0)).xyz;
        //vNormal = normalize(vNormal);

        gl_Position = mP * mV * vec4(vPosition, 1.0);

        //temp = vec4(boneTransform[boneIDs[0]][0]);
        //temp = boneIDs;

    } else {
        vPosition = (mM * vec4(position, 1.0)).xyz;
        vNormal = (transpose(inverse(mM)) * vec4(normal, 0.0)).xyz;
        vNormal = normalize(vNormal);

        gl_Position = mP * mV * vec4(vPosition, 1.0);
    }
}
