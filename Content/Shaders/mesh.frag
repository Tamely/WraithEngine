#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inWorldPos;
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
    vec4 texColor  = texture(sampler2D(baseColorImage, baseColorSampler), inTexCoord);
    vec4 baseColor = texColor * pushConstants.baseColorFactor;

    bool litMode = cameraFrame.renderOptions.x == 0u;
    bool hasLight = cameraFrame.lightColorAndEnabled.w > 0.5;

    if (!litMode) {
        outColor = vec4(baseColor.rgb, baseColor.a);
        return;
    }

    // Dielectric ambient is low; metallic gets a boosted ambient to stand in for
    // environment reflections that would normally light a mirror-like surface.
    float dielectricAmbient = mix(0.05, 0.20, pushConstants.roughness);
    float metallicAmbient   = mix(0.35, 0.15, pushConstants.roughness);
    float ambientFloor      = mix(dielectricAmbient, metallicAmbient, pushConstants.metallic);

    if (!hasLight) {
        outColor = vec4(baseColor.rgb * ambientFloor, baseColor.a);
        return;
    }

    vec3  L              = normalize(cameraFrame.lightDirectionAndIntensity.xyz);
    float lightIntensity = cameraFrame.lightDirectionAndIntensity.w;
    vec3  lightColor     = cameraFrame.lightColorAndEnabled.xyz;

    vec3  N = normalize(inNormal);
    vec3  V = normalize(cameraFrame.cameraPosition.xyz - inWorldPos);
    vec3  H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    // Roughness -> shininess: roughness=0 gives tight highlight, roughness=1 gives almost none.
    float shininess   = mix(256.0, 2.0, pushConstants.roughness);
    float specularStr = pow(NdotH, shininess) * (1.0 - pushConstants.roughness);

    // Metallic reduces diffuse (energy redirected to specular).
    float diffuseScale = mix(1.0, 0.15, pushConstants.metallic);
    float diffuse      = max(NdotL * lightIntensity * diffuseScale, ambientFloor);

    // Dielectric specular is near-white (F0 ~0.04); metallic tints specular with base color.
    vec3 specularColor = mix(vec3(0.04), baseColor.rgb, pushConstants.metallic);
    vec3 specular      = specularColor * specularStr * lightIntensity * lightColor;

    outColor = vec4(baseColor.rgb * lightColor * diffuse + specular, baseColor.a);
}
