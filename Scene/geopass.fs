#version 330

uniform vec3  diffuse_r;
uniform float alpha;
uniform float eta;
uniform float k_s;

in vec3 vNormal;    // serface normal in world space

layout (location = 0) out vec3 gNormal;
layout (location = 1) out vec3 gDiffuse_r;
layout (location = 2) out vec3 gAlpha;
layout (location = 3) out vec3 gConvert;

out vec4 fragColor;

void main() {
	// make sure all the g-buffer values are in the range of [0.1]
    gNormal = (vNormal + 1.0)/2.0; 
    gDiffuse_r = diffuse_r;
    gAlpha = vec3(alpha, eta, k_s);
    gConvert = vec3(0.0, 0.0, 0.0); // tells if value in gAlpha is flipped

    // because GL_RGBA8, the value has to be limited to 0-1,
    // so use 1/x if it is greater than 1.
    if (alpha > 1.0) {
    	gAlpha.x = 1.0/alpha;
    	gConvert.x = 1.0;
    }
    if (eta > 1.0) {
    	gAlpha.y = 1.0/eta;
    	gConvert.y = 1.0;
    }
    if (k_s > 1.0) {
    	gAlpha.z = 1.0/k_s;
    	gConvert.z = 1.0;
    }

    fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}