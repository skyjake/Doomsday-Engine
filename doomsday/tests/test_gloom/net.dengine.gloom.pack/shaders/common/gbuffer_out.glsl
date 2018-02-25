#ifndef GLOOM_GBUFFER_OUT_H
#define GLOOM_GBUFFER_OUT_H

layout (location = 1) out vec4 out_FragNormal;
layout (location = 2) out vec4 out_FragEmissive;

uniform mat3 uWorldToViewMatrix;

void GBuffer_SetFragNormal(vec3 worldNormal) {
    out_FragNormal = vec4(normalize(uWorldToViewMatrix * worldNormal) * 0.5 + vec3(0.5), 1.0);
}

void GBuffer_SetFragEmissive(vec3 emissiveColor) {
    out_FragEmissive = vec4(emissiveColor, 1.0);
}

#endif // GLOOM_GBUFFER_OUT_H
