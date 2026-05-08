#version 460

layout(push_constant) uniform GizmoPushConstants {
    mat4 viewProjection;
    vec4 startWorld;
    vec4 endWorld;
    vec4 color;
    vec2 viewportSize;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 startClip = push.viewProjection * vec4(push.startWorld.xyz, 1.0);
    vec4 endClip   = push.viewProjection * vec4(push.endWorld.xyz,   1.0);

    // Project both endpoints to pixel space so the perpendicular is
    // aspect-ratio-correct regardless of viewport dimensions.
    vec2 startPx = ((startClip.xy / startClip.w) * 0.5 + 0.5) * push.viewportSize;
    vec2 endPx   = ((endClip.xy   / endClip.w)   * 0.5 + 0.5) * push.viewportSize;

    vec2 dirPx  = endPx - startPx;
    float len   = length(dirPx);
    vec2 perpPx = (len > 0.001) ? vec2(-dirPx.y, dirPx.x) / len : vec2(0.0, 1.0);

    const float halfWidthPx = 5.0;
    vec2 offsetNDC = (perpPx * halfWidthPx / push.viewportSize) * 2.0;

    // Triangle-list quad (6 verts, 2 tris, no primitive restart):
    // tri0: (0,1,2) = (start+p, start-p, end+p)
    // tri1: (3,4,5) = (start-p, end+p,   end-p)
    int vi = gl_VertexIndex;
    bool isEnd   = (vi == 2 || vi >= 4);
    bool isRight = (vi == 1 || vi == 3 || vi == 5);

    vec4 clipPos = isEnd ? endClip : startClip;
    // Add NDC offset in clip space: clip.xy += ndcOffset * clip.w
    clipPos.xy += (isRight ? -offsetNDC : offsetNDC) * clipPos.w;
    clipPos.z = (clipPos.z + clipPos.w) * 0.5;

    gl_Position = clipPos;
    outColor = push.color;
}
