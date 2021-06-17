#version 330

uniform sampler2D image;
uniform sampler2D originalImage;

in vec2 geom_texCoord;

out vec4 fragColor;

void main() {
    vec4 orig = texture(originalImage, geom_texCoord);
    vec4 blur1 = textureLod(image, geom_texCoord, 1.0);
    vec4 blur2 = textureLod(image, geom_texCoord, 2.0);
    vec4 blur3 = textureLod(image, geom_texCoord, 3.0);
    vec4 blur4 = textureLod(image, geom_texCoord, 4.0);

    fragColor = 0.8843*orig + 0.1*blur1 + 0.012*blur2 + 0.0027*blur3 + 0.001*blur4;
    //fragColor = blur4;
}