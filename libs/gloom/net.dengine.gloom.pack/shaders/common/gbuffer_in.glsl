#ifndef GLOOM_GBUFFER_IN_H
#define GLOOM_GBUFFER_IN_H

layout (pixel_center_integer) in vec4 gl_FragCoord;

#include "gbuffer.glsl"

uniform sampler2D uGBufferDiffuse;
uniform sampler2D uGBufferEmissive;
uniform sampler2D uGBufferSpecGloss;
uniform sampler2D uGBufferNormal;
uniform sampler2D uGBufferDepth;

/**
 * Calculates the view space position of a fragment at normalized
 * screen coordinates.
 *
 * @param coord  G-buffer texture coordinate.
 */
vec4 GBuffer_ViewSpacePos(vec2 coord) {
    float depth = texture(uGBufferDepth, coord).r;
    return GBuffer_ViewSpacePosFromDepth(coord, depth);
}

vec4 GBuffer_FragViewSpacePos(void) {
    // Read the fragment depth from the Z buffer.
    float depth = texelFetch(uGBufferDepth, ivec2(gl_FragCoord.xy), 0).r;
    return GBuffer_ViewSpacePosFromDepth(GBuffer_NormalizedFragCoord(), depth);
}

vec3 GBuffer_FragViewSpaceNormal(void) {
    vec4 packedNormal = texelFetch(uGBufferNormal, ivec2(gl_FragCoord.xy), 0);
    if (packedNormal.xyz == vec3(0.0)) return vec3(0.0);
    return GBuffer_UnpackNormal(packedNormal);
}

// MaterialData GBuffer_FragMaterialData(void) {
//     vec4 data = texelFetch(uGBufferMaterial, ivec2(gl_FragCoord.xy), 0);
//     return MaterialData(uint(data.b + 0.5), data.rg);
// }

vec3 GBuffer_FragDiffuse(void) {
    return texelFetch(uGBufferDiffuse, ivec2(gl_FragCoord.xy), 0).rgb;
}

vec3 GBuffer_FragEmissive(void) {
    return texelFetch(uGBufferEmissive, ivec2(gl_FragCoord.xy), 0).rgb;
}

vec4 GBuffer_FragSpecGloss(void) {
    return texelFetch(uGBufferSpecGloss, ivec2(gl_FragCoord.xy), 0);
}

#endif // GLOOM_GBUFFER_IN_H
