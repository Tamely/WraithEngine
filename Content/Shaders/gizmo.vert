#version 460

layout(push_constant) uniform GizmoPushConstants {
    mat4 viewProjection;
    vec4 startWorld;
    vec4 endWorld;
    vec4 color;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 worldPos = (gl_VertexIndex == 0) ? push.startWorld : push.endWorld;
    worldPos.w = 1.0;
    vec4 clipPos = push.viewProjection * worldPos;
    clipPos.z = (clipPos.z + clipPos.w) * 0.5;
    gl_Position = clipPos;
    outColor = push.color;
}
