#version 330

const int displayMode = 0;

const int displayImage = 0;
const int displayWordPos = 1;
const int displayLightNDC = 2;
const int displayDepthBuffer = 3;
const int displayShadowMap = 4;
const int displayFragNormal = 5;
const int displayDiffuseReflectance = 6;


const float PI = 3.14159265358979323846264;

// g-buffer textures
uniform sampler2D gNormal;
uniform sampler2D gDiffuse_r;
uniform sampler2D gAlpha;
uniform sampler2D gConvert;
uniform sampler2D gDepth;
uniform sampler2D shadowMap;

uniform float windowWidth;
uniform float windowHeight;

uniform mat4 mV;     // View matrix from camera view
uniform mat4 mP;     // Projection matrix from camera view
uniform mat4 mV_l;   // View matrix from light view
uniform mat4 mP_l;   // Projection matrix from light view

uniform vec3  lightPosition; // light position in word space
uniform vec3  lightPower;
uniform vec3  cameraEye;     // camera eye position in world space.

in vec2 geom_texCoord;

out vec4 fragColor;

// function from microfacet.fs
float isotropicMicrofacet(vec3 i, vec3 o, vec3 n, float eta, float alpha);

// Make the depth linear for display.
// The formular can be found here: 
//   https://learnopengl.com/Advanced-OpenGL/Depth-testing 
float LinearDepth(float depth) 
{
    float near = 1.0; 
    float far  = 80.0; 
    float d = depth * 2.0 - 1.0; 
    float linear_d = (2.0 * near * far) / (far + near - d * (far - near)); 
    return linear_d / far; 
}

void main() {
    // unpacking the texture 
    vec4 viewport = vec4(0.0, 0.0, windowWidth, windowHeight);
    vec3 tNormal = texture(gNormal, geom_texCoord).xyz;
    vec3 tDiffuse_r = texture(gDiffuse_r, geom_texCoord).xyz;
    vec3 tAlpha = texture(gAlpha, geom_texCoord).xyz;
    vec3 tConvert = texture(gConvert, geom_texCoord).xyz;
    vec3 tDepth = texture(gDepth, geom_texCoord).xyz;

    // convert values from the texture to the original 
    float alpha = tAlpha.x;
    float eta = tAlpha.y;
    float k_s = tAlpha.z;

    if (tConvert.x > 0.0) {
      alpha = 1.0/tAlpha.x;
    }
    if (tConvert.y > 0.0) {
      eta = 1.0/tAlpha.y;
    }
    if (tConvert.z > 0.0) {
      k_s = 1.0/tAlpha.z;
    }
    vec3 snormal = (gl_FrontFacing) ? tNormal*2.0-1.0 : -(tNormal*2.0-1.0);
    snormal = normalize(snormal);

    //get the position of the fragment in world space
    vec4 ndcPos;
    ndcPos.xy = 2.0 * gl_FragCoord.xy / viewport.zw - 1.0;
    ndcPos.z = 2.0 * tDepth.r - 1.0;
    ndcPos.w = 1.0;
    vec4 eyePos = inverse(mP) * ndcPos;    
    vec3 vPos = (inverse(mV) * (eyePos/eyePos.w)).xyz; // world space  

    // get the shadow texture coordinates and depth from the shadow map
    vec4 clipPos_l = mP_l * mV_l * vec4(vPos, 1.0);
    vec4 ndcPos_l = clipPos_l/clipPos_l.w;
    vec4 shadowTextCoord = ndcPos_l * 0.5  + 0.5;  
    vec3 tLight_Depth = texture(shadowMap, shadowTextCoord.xy).xyz;

    // display some buffers for debug 
    if (displayMode == displayWordPos) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        fragColor.rgb += vPos.xyz;

    } else if (displayMode == displayLightNDC) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        fragColor.rgb += ndcPos_l.xyz;

    } else if (displayMode == displayDepthBuffer) {
        float d = LinearDepth(tDepth.r); 
        fragColor = vec4(vec3(d), 1.0);

    } else if (displayMode == displayShadowMap) {
        float sd = LinearDepth(tLight_Depth.r); 
        fragColor = vec4(vec3(sd), 1.0);

     } else if (displayMode == displayFragNormal) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        fragColor.rgb += snormal.xyz;

    } else if (displayMode == displayDiffuseReflectance) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        fragColor.rgb += tDiffuse_r.xyz;

    } else { // display image 

        // if shadow, color will be black
        if (tLight_Depth.r < shadowTextCoord.z - 0.0001) {
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        // else, get intensity of the light
        vec3 lightDir1 = normalize(vPos - lightPosition);
        vec3 I = lightPower / (4.0 * PI);

        // distance of the light from hit point
        float r = length(lightPosition - vPos);

        // w dot n
        float NdotW = max(dot(normalize(snormal), normalize(-lightDir1)), 0.0);

        // get BRDF
        float specular = isotropicMicrofacet(normalize(-lightDir1), normalize(cameraEye-vPos), normalize(snormal), eta, alpha);
        vec3 brdf = k_s*specular + tDiffuse_r;
        //vec3 brdf = tDiffuse_r/PI;

        // total illumination
        vec3 Lr = I * brdf * NdotW/(r * r);
        vec3 Lr_all = vec3(min(max(Lr.x, 0.0), 1.0), min(max(Lr.y, 0.0), 1.0), min(max(Lr.z, 0.0), 1.0));

        fragColor = vec4(Lr_all, 1.0);
    }
 }