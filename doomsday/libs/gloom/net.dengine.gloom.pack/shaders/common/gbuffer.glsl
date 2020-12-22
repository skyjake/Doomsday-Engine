#ifndef GLOOM_GBUFFER_H
#define GLOOM_GBUFFER_H

uniform mat4 uInverseProjMatrix;
uniform vec2 uViewportSize;

vec4 GBuffer_PackNormal(vec3 normal) {
    return vec4(normal.xyz * 0.5 + 0.5, 1.0);
}

vec3 GBuffer_UnpackNormal(vec4 packedNormal) {
    return packedNormal.xyz * 2.0 - 1.0;
}

vec2 GBuffer_NormalizedFragCoord(void) {
    return gl_FragCoord.xy / uViewportSize;
}

vec4 GBuffer_ViewSpacePosFromDepth(vec2 normCoord, float depth) {
    float z = depth * 2.0 - 1.0;
    // Reverse projection to view space.
    vec4 clipSpacePos = vec4(normCoord * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePos = uInverseProjMatrix * clipSpacePos;
    vec3 pos = viewSpacePos.xyz / viewSpacePos.w;
    return vec4(pos, 1.0);
}

#endif // GLOOM_GBUFFER_H
