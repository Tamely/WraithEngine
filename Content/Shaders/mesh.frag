#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform texture2D baseColorImage;
layout(set = 0, binding = 2) uniform sampler baseColorSampler;

layout(push_constant) uniform MeshGraphicsPushConstants {
    mat4  model;
    vec4  baseColorFactor;
    float metallic;
    float roughness;
} pushConstants;
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
    vec4 texColor   = texture(sampler2D(baseColorImage, baseColorSampler), inTexCoord);
    vec4 baseColor  = texColor * pushConstants.baseColorFactor;

    float lighting = 1.0;
    vec3 tint = vec3(1.0);

    bool litMode = cameraFrame.renderOptions.x == 0u;
    bool hasLight = cameraFrame.lightColorAndEnabled.w > 0.5;

    if (litMode) {
        float ambientFloor = mix(0.05, 0.20, pushConstants.roughness);
        if (hasLight) {
            vec3  lightDir       = normalize(cameraFrame.lightDirectionAndIntensity.xyz);
            float lightIntensity = cameraFrame.lightDirectionAndIntensity.w;
            tint = cameraFrame.lightColorAndEnabled.xyz;
            float diffuse = max(dot(normalize(inNormal), lightDir), 0.0);
            float lit     = max(diffuse * lightIntensity, ambientFloor);
            // Metallic surfaces reflect the light color more strongly.
            float specularBoost = pushConstants.metallic * max(diffuse, 0.0) * lightIntensity * 0.4;
            lighting = lit + specularBoost;
        } else {
            lighting = ambientFloor;
        }
    }

    outColor = vec4(baseColor.rgb * tint * lighting, baseColor.a);
}
