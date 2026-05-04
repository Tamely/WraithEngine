#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(0.35, 0.7, 0.2));
    float lighting = max(dot(normalize(inNormal), lightDir), 0.2);
    outColor = vec4(vec3(0.75, 0.82, 0.92) * lighting, 1.0);
}
