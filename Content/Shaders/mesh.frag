#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform texture2D baseColorImage;
layout(set = 0, binding = 2) uniform sampler baseColorSampler;
layout(std140, set = 0, binding = 0) uniform CameraFrame {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 viewportSize;
    uvec4 renderOptions;
} cameraFrame;

void main() {
    vec3 lightDir = normalize(vec3(0.35, 0.7, 0.2));
    vec4 baseColor = texture(sampler2D(baseColorImage, baseColorSampler), inTexCoord);
    float lighting = 1.0;
    if (cameraFrame.renderOptions.x == 0u) {
        lighting = max(dot(normalize(inNormal), lightDir), 0.2);
    }
    outColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
