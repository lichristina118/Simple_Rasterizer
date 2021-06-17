#version 330

// function from sunsky.fs
vec3 sunskyRadiance(vec3 dir);

const int displayMode = 0;

const int displayImage = 0;
const int displayEyeSpacePos = 1;
const int displayEyeSpaceNormal = 2;

const float PI = 3.14159265358979323846264;

// g-buffer textures
uniform sampler2D gNormal;
uniform sampler2D gDiffuse_r;
uniform sampler2D gDepth;

uniform float windowWidth;
uniform float windowHeight;

uniform mat4 mV;     // View matrix from camera view
uniform mat4 mP;     // Projection matrix from camera view

uniform vec3  lightRadiance;
uniform float lightRange;

in vec2 geom_texCoord;

out vec4 fragColor;

const int num_samples = 500;
float oc_scale = lightRange;


float random(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// get a sample inside a hemisphere
vec3 getSample(int i, vec3 pos) {
    //generate a random point p in the 2D circle
    float r = random(i + pos.xy);
    r = r * r;
    float theta = 2.0 * PI* random(i + pos.xz);

    //map point p to the 3D hemisphere
    vec3 p;
    p.x = r*cos(theta);
    p.y = r*sin(theta);
    p.z = sqrt(1-p.x*p.x-p.y*p.y);

    //make it inside the hemisphere
    float c = random(i + pos.yz);
    return oc_scale * c*p;
}

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

void main() {
    // unpacking the texture 
    vec4 viewport = vec4(0.0, 0.0, windowWidth, windowHeight);
    vec3 tNormal = texture(gNormal, geom_texCoord).xyz;
    vec3 tDiffuse_r = texture(gDiffuse_r, geom_texCoord).xyz;
    vec3 tDepth = texture(gDepth, geom_texCoord).xyz;

    vec3 worldSpaceNormal = (gl_FrontFacing) ? tNormal*2.0-1.0 : -(tNormal*2.0-1.0);

    //if (worldSpaceNormal == vec3(0.0, 0.0, 0.0)) {
    //    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    //}

    // surface normal in eye space
    vec3 eyeSpaceNormal = (transpose(inverse(mV)) * vec4(worldSpaceNormal, 0.0)).xyz;    
    eyeSpaceNormal = normalize(eyeSpaceNormal);


    //get the position of the fragment in eye space
    vec3 eyeSpacePos = screenSpaceToEyeSpace(vec3(gl_FragCoord.x, gl_FragCoord.y, tDepth.r)); 

    // display some buffers for debug
    if (displayMode == displayEyeSpacePos) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        fragColor.rgb += eyeSpacePos.xyz;

    } else if (displayMode == displayEyeSpaceNormal) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        fragColor.rgb += eyeSpaceNormal.xyz;

    } else { // display image 
        //get the tangent vectors for the surface frame
        vec3 t0, t1;
        if (eyeSpaceNormal.x == 0.0 && eyeSpaceNormal.y == 0.0) {
            t0 = vec3(1.0, 0.0, 0.0);
            t1 = vec3(0.0, 1.0, 0.0);
        } else {
            //dim1: y-axis
            t1.x =  eyeSpaceNormal.y/sqrt(eyeSpaceNormal.x * eyeSpaceNormal.x + eyeSpaceNormal.y * eyeSpaceNormal.y);
            t1.y = -eyeSpaceNormal.x/sqrt(eyeSpaceNormal.x * eyeSpaceNormal.x + eyeSpaceNormal.y * eyeSpaceNormal.y);
            t1.z = 0;

            //dim0: x-axis
            t0 = cross(t1, eyeSpaceNormal);
         }

        int total_unoccluded = 0;
        for(int i = 0; i < num_samples; ++i) {
            // pick a sample point from serface frame
            vec3 sample = getSample(i, eyeSpacePos);

            // convert sample to eye space
            vec3 eyeSpaceSample = eyeSpacePos + sample.x*t0 + sample.y*t1 + sample.z*eyeSpaceNormal;

            // get depth from the g-buffer for the sample point
            vec4 tmp = (mP * vec4(eyeSpaceSample, 1.0));
            vec3 ndcSample = tmp.xyz / tmp.w;
            vec3 screenSpaceSample = 0.5 * ndcSample + 0.5;
            float depth = texture(gDepth, screenSpaceSample.xy).x;

            // check if this sample point is occluded or not
            if (screenSpaceSample.z <= depth + 0.0001 ) {
                total_unoccluded = total_unoccluded + 1;
            } else {
                // the sample point is counted as unoccluded if the distance 
                // between the point on the depth buffer and the current 
                // fragment is greater than the occlussion range
                // defined by the ambient light. 
                vec3 p_depth = vec3(screenSpaceSample.x, screenSpaceSample.y, depth);
                if (length(screenSpaceToEyeSpace(p_depth) - eyeSpacePos) > 4) {
                    total_unoccluded = total_unoccluded + 1;
                }
            }
        }
        float oc_factor = total_unoccluded/num_samples;
        vec3 Lr = oc_factor * lightRadiance * tDiffuse_r;

        fragColor = vec4(Lr, 1.0);
    }
}