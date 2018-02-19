#ifndef GLOOM_GBUFFER_IN_H
#define GLOOM_GBUFFER_IN_H

layout (pixel_center_integer) in vec4 gl_FragCoord;

uniform mat4 uInverseProjMatrix;

uniform sampler2D uGBufferAlbedo;
uniform sampler2D uGBufferNormal;
uniform sampler2D uGBufferDepth;

vec4 GBuffer_FragmentViewSpacePos(void) {
    // Read the fragment depth from the Z buffer.
    float depth = texelFetch(uGBufferDepth, ivec2(gl_FragCoord.xy), 0).r;
    float z = depth * 2.0 - 1.0;
    // Reverse projection to view space.
    vec2 normCoord = gl_FragCoord.xy / vec2(textureSize(uGBufferDepth, 0));
    vec4 clipSpacePos = vec4(normCoord * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePos = uInverseProjMatrix * clipSpacePos;
    vec3 pos = viewSpacePos.xyz / viewSpacePos.w;
    return vec4(pos, 1.0);
}

#endif // GLOOM_GBUFFER_IN_H
