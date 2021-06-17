#version 330

const float PI = 3.14159265358979323846264;

uniform float windowWidth;
uniform float windowHeight;

uniform mat4 mM;  // Model matrix
uniform mat4 mV;  // View matrix
uniform mat4 mP;  // Projection matrix

uniform vec3  lightPosition; // light position in word space
uniform vec3  lightPower;
uniform vec3  diffuse_r;
uniform float alpha;
uniform float eta;
uniform float k_s;
uniform vec3  cameraEye;     // camera eye position in world space

in vec3 vNormal;    // serface normal in world space
in vec3 vPosition;  // vertex position in world space

uniform samplerCube skybox;
uniform int skyboxReflection;

out vec4 fragColor;

//in vec4 temp;

// function from microfacet.fs
float isotropicMicrofacet(vec3 i, vec3 o, vec3 n, float eta, float alpha);

void main() {
    
    vec3 snormal = (gl_FrontFacing) ? vNormal : -vNormal;

    //get the position of the pixel in workd space
    /*********************
    * this part is copied from 
    *   https://www.khronos.org/opengl/wiki/Compute_eye_space_from_window_space 
    */
    vec4 viewport = vec4(0.0, 0.0, windowWidth, windowHeight);
    vec4 ndcPos;
    ndcPos.xy = ((2.0 * gl_FragCoord.xy) - (2.0 * viewport.xy)) / (viewport.zw) - 1;
    ndcPos.z = (2.0 * gl_FragCoord.z - gl_DepthRange.near - gl_DepthRange.far) / (gl_DepthRange.far - gl_DepthRange.near);
    ndcPos.w = 1.0;
    vec4 clipPos = ndcPos / gl_FragCoord.w;
    vec4 eyePos = inverse(mP) * clipPos;
    /**********************/


    // get to the world space
    vec3 vPos = (inverse(mV) * eyePos).xyz;

    // get intensity of the light
    vec3 lightDir1 = normalize(vPos - lightPosition);
    vec3 I = lightPower / (4.0 * PI);

    // distance of the light from hit point
    float r = length(lightPosition - vPos);

    // w dot n
    float NdotW = max(dot(normalize(snormal), normalize(-lightDir1)), 0.0);

    // get BRDF
    float specular = isotropicMicrofacet(normalize(-lightDir1), normalize(cameraEye-vPos), normalize(snormal), eta, alpha);
    vec3 brdf = k_s*specular + diffuse_r;

    // total illumination
    vec3 Lr = I * brdf * NdotW/(r * r);

    if (skyboxReflection != 1) {    
        vec3 Lr_all = vec3(min(max(Lr.x, 0.0), 1.0), min(max(Lr.y, 0.0), 1.0), min(max(Lr.z, 0.0), 1.0));

        fragColor = vec4(Lr_all, 1.0);
    } else {
        // add skybox mirrow reflection
        vec3 I1 = normalize(vPosition - cameraEye);
        vec3 R = reflect(I1, normalize(snormal));
        vec3 Lm = texture(skybox, R).rgb + Lr;
        vec3 Lm_all = vec3(min(max(Lm.x, 0.0), 1.0), min(max(Lm.y, 0.0), 1.0), min(max(Lm.z, 0.0), 1.0));

        fragColor = vec4(Lm_all, 1.0);
    }
}