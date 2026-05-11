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
    vec4 lightDirectionAndIntensity; // xyz = direction, w = intensity
    vec4 lightColorAndEnabled;       // xyz = color, w = 1.0 if dynamic light active
} cameraFrame;

void main() {
    vec4 baseColor = texture(sampler2D(baseColorImage, baseColorSampler), inTexCoord);
    float lighting = 1.0;
    vec3 tint = vec3(1.0);

    bool litMode = cameraFrame.renderOptions.x == 0u;
    bool hasLight = cameraFrame.lightColorAndEnabled.w > 0.5;

    if (litMode) {
        if (hasLight) {
            vec3 lightDir = normalize(cameraFrame.lightDirectionAndIntensity.xyz);
            float lightIntensity = cameraFrame.lightDirectionAndIntensity.w;
            tint = cameraFrame.lightColorAndEnabled.xyz;
            float diffuse = max(dot(normalize(inNormal), lightDir), 0.0);
            lighting = max(diffuse * lightIntensity, 0.15);
        } else {
            // No active light: ambient-only so hiding a light has a visible effect.
            lighting = 0.15;
        }
    }

    outColor = vec4(baseColor.rgb * tint * lighting, baseColor.a);
}
