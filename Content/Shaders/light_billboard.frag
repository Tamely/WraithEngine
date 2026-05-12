#version 460

layout(set = 0, binding = 0) uniform sampler2D iconTexture;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 texel = texture(iconTexture, inUv);
    outColor = vec4(inColor.rgb * texel.rgb, inColor.a * texel.a);
}
