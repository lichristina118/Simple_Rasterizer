#version 330

uniform sampler2D gNormal;
uniform sampler2D gDiffuse_r;
uniform sampler2D gAlpha;
uniform sampler2D gConvert;

uniform float windowWidth;
uniform float windowHeight;

uniform mat4 mV;  // View matrix
uniform mat4 mP;  // Projection matrix

uniform vec3  lightPosition; // light position in word space
uniform vec3  lightPower;
uniform vec3  cameraEye;     // camera eye position in world space.

in vec2 geom_texCoord;

out vec4 fragColor;

void main() {
    // unpacking the texture
    vec3 tNormal = texture(gNormal, geom_texCoord).xyz;
    vec3 tDiffuse_r = texture(gDiffuse_r, geom_texCoord).xyz;
    vec3 tAlpha = texture(gAlpha, geom_texCoord).xyz;
    vec3 tConvert = texture(gConvert, geom_texCoord).xyz;
    float alpha = tAlpha.x;
    float eta = tAlpha.y;
    float k_s = tAlpha.z;

    // convert the to original value
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

    // fixed light dir
    vec3 lightDir = vec3(-1.0, -1.0, -1.0);

    // w dot n
    float NdotW = max(dot(normalize(snormal), normalize(-lightDir)), 0.0);

    vec3 Lr = NdotW * tDiffuse_r;

    vec3 Lr_all = vec3(min(max(Lr.x, 0.0), 1.0), min(max(Lr.y, 0.0), 1.0), min(max(Lr.z, 0.0), 1.0));

    fragColor = vec4(Lr_all, 1.0);

    // debug: show normals with color
    //fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    //fragColor.rgb += abs(snormal.xyz);
 }