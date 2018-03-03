#ifndef GLOOM_GBUFFER_IN_H
#define GLOOM_GBUFFER_IN_H

layout (pixel_center_integer) in vec4 gl_FragCoord;

uniform mat4 uInverseProjMatrix;

// uniform sampler2D uGBufferMaterial;
uniform sampler2D uGBufferDiffuse;
uniform sampler2D uGBufferEmissive;
uniform sampler2D uGBufferSpecGloss;
uniform sampler2D uGBufferNormal;
uniform sampler2D uGBufferDepth;

// struct MaterialData {
//     uint matIndex;
//     vec2 uv;
// };

vec4 GBuffer_ViewSpacePosFromDepth(vec2 normCoord, float depth) {
    float z = depth * 2.0 - 1.0;
    // Reverse projection to view space.
    vec4 clipSpacePos = vec4(normCoord * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePos = uInverseProjMatrix * clipSpacePos;
    vec3 pos = viewSpacePos.xyz / viewSpacePos.w;
    return vec4(pos, 1.0);
}

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
    vec2 normCoord = gl_FragCoord.xy / vec2(textureSize(uGBufferDepth, 0));
    return GBuffer_ViewSpacePosFromDepth(normCoord, depth);
}

vec3 GBuffer_FragViewSpaceNormal(void) {
    vec3 norm = texelFetch(uGBufferNormal, ivec2(gl_FragCoord.xy), 0).rgb;
    if (norm == vec3(0.0)) return norm;
    return norm * 2.0 - 1.0;
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
