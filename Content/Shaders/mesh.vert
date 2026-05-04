#version 460

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;

layout(std140, set = 0, binding = 0) uniform CameraFrame {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 viewportSize;
    uvec4 renderOptions;
} cameraFrame;

layout(push_constant) uniform MeshGraphicsPushConstants {
    mat4 model;
} pushConstants;

void main() {
    vec4 worldPosition = pushConstants.model * vec4(inPosition.xyz, 1.0);
    vec4 clipPosition = cameraFrame.viewProjection * worldPosition;
    clipPosition.z = (clipPosition.z + clipPosition.w) * 0.5;
    gl_Position = clipPosition;
    outNormal = normalize(mat3(pushConstants.model) * inNormal.xyz);
    outTexCoord = inTexCoord;
}
