#ifndef GLOOM_GBUFFER_H
#define GLOOM_GBUFFER_H

vec4 GBuffer_PackNormal(vec3 normal) {
    return vec4(normal.xyz * 0.5 + 0.5, 1.0);
}

vec3 GBuffer_UnpackNormal(vec4 packedNormal) {
    return packedNormal.xyz * 2.0 - 1.0;
}

#endif // GLOOM_GBUFFER_H
