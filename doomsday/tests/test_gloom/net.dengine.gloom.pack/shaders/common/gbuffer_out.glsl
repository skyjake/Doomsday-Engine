#ifndef GLOOM_GBUFFER_OUT_H
#define GLOOM_GBUFFER_OUT_H

layout (location = 1) out vec4 out_FragNormal;
layout (location = 2) out vec4 out_FragEmissive;
layout (location = 3) out vec4 out_FragSpecGloss;

uniform mat3 uWorldToViewRotate;

// void GBuffer_SetFragMaterial(uint matIndex, vec2 texCoord) {
//     out_FragColor = vec4(texCoord.s, texCoord.t, float(matIndex), 1.0);
// }

void GBuffer_SetFragDiffuse(vec4 diffuse) {
    out_FragColor = diffuse;
}

void GBuffer_SetFragEmissive(vec3 emissive) {
    out_FragEmissive = vec4(emissive, 1.0);
}

void GBuffer_SetFragSpecGloss(vec4 specGloss) {
    out_FragSpecGloss = specGloss;
}

void GBuffer_SetFragNormal(vec3 worldNormal) {
    out_FragNormal = vec4(normalize(uWorldToViewRotate * worldNormal) * 0.5 + vec3(0.5), 1.0);
}

#endif // GLOOM_GBUFFER_OUT_H
