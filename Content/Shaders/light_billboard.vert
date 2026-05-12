#version 460

layout(push_constant) uniform BillboardPushConstants {
    mat4 viewProjection;
    vec4 worldPositionAndHalfSize;
    vec4 color;
    vec4 cameraRight;
    vec4 cameraUp;
} push;

layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outColor;

void main() {
    const vec2 corners[6] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0,  1.0)
    );

    vec2 corner = corners[gl_VertexIndex];
    vec3 worldPosition = push.worldPositionAndHalfSize.xyz +
                         push.cameraRight.xyz * corner.x * push.worldPositionAndHalfSize.w +
                         push.cameraUp.xyz * corner.y * push.worldPositionAndHalfSize.w;

    gl_Position = push.viewProjection * vec4(worldPosition, 1.0);
    outUv = vec2(corner.x * 0.5 + 0.5, 1.0 - (corner.y * 0.5 + 0.5));
    outColor = push.color;
}
