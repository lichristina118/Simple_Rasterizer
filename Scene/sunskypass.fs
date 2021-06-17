#version 330

// function from sunsky.fs
vec3 sunskyRadiance(vec3 dir);

const float PI = 3.14159265358979323846264;

// g-buffer textures
uniform sampler2D gNormal;
uniform sampler2D gDepth;

uniform float windowWidth;
uniform float windowHeight;

uniform mat4 mV;     // View matrix from camera view
uniform mat4 mP;     // Projection matrix from camera view
uniform vec3 cameraEye; 

in vec2 geom_texCoord;

out vec4 fragColor;

// convert a point from screen sapce to eye space
vec3 screenSpaceToEyeSpace(vec3 point) {
    vec4 ndcPos;
    vec4 viewport = vec4(0.0, 0.0, windowWidth, windowHeight);
    ndcPos.xy = 2.0 * point.xy / viewport.zw - 1.0;
    ndcPos.z = 2.0 * point.z - 1.0;
    ndcPos.w = 1.0;
    vec4 clip = inverse(mP) * ndcPos;    
    vec3 eyeSpacePos = (clip/clip.w).xyz;
    return eyeSpacePos;      
}

// get the world space view ray direction
vec3 getViewDir(vec2 fragCoord) {
    // convert the fragment on near and far plane to world space 
    vec3 pos_near = screenSpaceToEyeSpace(vec3(fragCoord, -1));
    vec3 pos_far = screenSpaceToEyeSpace(vec3(fragCoord, 1));
    pos_near = (inverse(mV) * vec4(pos_near, 1.0)).xyz; 
    pos_far = (inverse(mV) * vec4(pos_far, 1.0)).xyz;

    return normalize(pos_near - pos_far);    
}

vec3 view_ray() {
    vec4 near = vec4(
    2.0 * ( (gl_FragCoord.x) / windowWidth - 0.5),
    2.0 * ( (gl_FragCoord.y) / windowHeight - 0.5),
        0.0,
        1.0
    );
    near = inverse(mV) * inverse(mP) * near ;
    vec4 far = near +  (inverse(mV) * inverse(mP))[2];
    near.xyz /= near.w ;
    far.xyz /= far.w ;
    return normalize(far.xyz-near.xyz) ;
}

void main() {
    // unpacking the depth from g-buffer     
    vec3 tDepth = texture(gDepth, geom_texCoord).xyz;    
    vec3 tNormal = texture(gNormal, geom_texCoord).xyz;    
    vec3 snormal = (gl_FrontFacing) ? tNormal*2.0-1.0 : -(tNormal*2.0-1.0);    
    //snormal = normalize(snormal);       
    if (tDepth.r == 1.0) {        
        //get the position of the fragment in eye space        
        vec3 eyeSpacePos = screenSpaceToEyeSpace(vec3(gl_FragCoord.x, gl_FragCoord.y, tDepth.r));         
        // add sun-sky radiance        
        vec3 wPos = (inverse(mV) * vec4(eyeSpacePos, 1.0)).xyz;        
        vec3 Lr = sunskyRadiance(normalize(wPos - cameraEye));        
        //vec3 Lr = sunskyRadiance(view_ray());        
        fragColor = vec4(Lr, 1.0);    
    } else {        
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);   
    }
}