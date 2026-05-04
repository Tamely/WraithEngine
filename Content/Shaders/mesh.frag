#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform texture2D baseColorImage;
layout(set = 0, binding = 2) uniform sampler baseColorSampler;

void main() {
    vec3 lightDir = normalize(vec3(0.35, 0.7, 0.2));
    float lighting = max(dot(normalize(inNormal), lightDir), 0.2);
    vec4 baseColor = texture(sampler2D(baseColorImage, baseColorSampler), inTexCoord);
    outColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
