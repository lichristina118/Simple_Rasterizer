#version 330

uniform mat4 mV;     // View matrix from camera view
uniform mat4 mP;     // Projection matrix from camera view
uniform vec3  cameraEye;  // camera eye position in world space.

uniform float windowWidth;
uniform float windowHeight;

in vec2 geom_texCoord;

// g-buffer textures
uniform sampler2D gDepth;
uniform sampler2D gNormal;

// skybox texture
uniform samplerCube skybox;

out vec4 fragColor;

void main()
{    
    // unpack the normal and depth from the g-buffers 
    vec3 tDepth = texture(gDepth, geom_texCoord).xyz;
    vec3 tNormal = texture(gNormal, geom_texCoord).xyz;
    vec3 snormal = (gl_FrontFacing) ? tNormal*2.0-1.0 : -(tNormal*2.0-1.0);


    if (tDepth.r < 1.0) {
    	// get world space position
    	vec4 ndcPos;
    	vec4 viewport = vec4(0.0, 0.0, windowWidth, windowHeight);
    	ndcPos.xy = 2.0 * gl_FragCoord.xy / viewport.zw - 1.0;
    	ndcPos.z = 2.0 * tDepth.r - 1.0;
    	ndcPos.w = 1.0;
    	vec4 eyePos = inverse(mP) * ndcPos;    
    	vec3 vPos = (inverse(mV) * (eyePos/eyePos.w)).xyz; // world space  

  		// skybox mirror reflection 
      	vec3 I = normalize(vPos - cameraEye);
    	vec3 R = reflect(I, normalize(snormal));
    	vec3 Lm = texture(skybox, R).rgb;

		fragColor = vec4(Lm, 1.0);
	} else {
		// background
		fragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}
}