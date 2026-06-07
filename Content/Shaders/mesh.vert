#version 460

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) flat out uint outObjectIndex;

layout(std140, set = 0, binding = 0) uniform CameraFrame {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 viewportSize;
    uvec4 renderOptions;
} cameraFrame;

struct MeshGraphicsObjectData {
    mat4 model;
    vec4 baseColorFactor;
    vec4 materialParams;
};

layout(std430, set = 0, binding = 1) readonly buffer ObjectData {
    MeshGraphicsObjectData objects[];
} objectData;

layout(push_constant) uniform MeshGraphicsPushConstants {
    mat4  model;
    vec4  baseColorFactor;
    float metallic;
    float roughness;
    uvec4 drawOptions;
} pushConstants;

void main() {
    uint objectBase = pushConstants.drawOptions.x;
    outObjectIndex = objectBase == 0xffffffffu
        ? 0xffffffffu
        : objectBase + gl_InstanceIndex;
    mat4 model = objectBase == 0xffffffffu
        ? pushConstants.model
        : objectData.objects[outObjectIndex].model;

    vec4 worldPosition = model * vec4(inPosition.xyz, 1.0);
    vec4 clipPosition = cameraFrame.viewProjection * worldPosition;
    clipPosition.z = (clipPosition.z + clipPosition.w) * 0.5;
    gl_Position = clipPosition;
    outNormal = normalize(mat3(model) * inNormal.xyz);
    outTexCoord = inTexCoord;
    outWorldPos = worldPosition.xyz;
}
