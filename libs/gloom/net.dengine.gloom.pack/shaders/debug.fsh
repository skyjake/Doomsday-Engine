#include "common/gbuffer_in.glsl"

uniform sampler2D uSSAOBuf;
uniform sampler2D uShadowMap;
uniform sampler2D uBloomFramebuf;
uniform sampler2D uBrightnessSamples;
uniform mat4 uViewToLightMatrix;
uniform vec3 uViewSpaceLightDir;
uniform int uDebugMode;

in vec2 vUV;

/*float textureShadowMap(vec2 uv) {
    vec2 fpos = uv * textureSize(uShadowMap, 0);
    ivec2 pos = ivec2(fpos);
    float values[4] = float[4] (
        texelFetch(uShadowMap, pos, 0).r,
        texelFetch(uShadowMap, pos + ivec2(1, 0), 0).r,
        texelFetch(uShadowMap, pos + ivec2(0, 1), 0).r,
        texelFetch(uShadowMap, pos + ivec2(1, 1), 0).r
    );
    vec2 inter = fract(fpos);
    vec2 mixed = vec2(mix(values[0], values[1], inter.x),
                      mix(values[2], values[3], inter.x));
    return mix(mixed.s, mixed.t, inter.y);
}*/
#if 0
const vec3 pcfWeights[9] = vec3[9] (
    vec3( 0,  0, 2.4),
    vec3(-1, -1, 1),
    vec3( 0, -1, 1.4),
    vec3( 1, -1, 1),
    vec3(-1,  0, 1.4),
    vec3( 1,  0, 1.4),
    vec3(-1,  1, 1),
    vec3( 0,  1, 1.4),
    vec3( 1,  1, 1)
);

float Gloom_FetchShadow(vec4 lightSpacePos, float dp) {
    vec3 pos = (lightSpacePos.xyz / lightSpacePos.w) * 0.5 + 0.5;
    float pointDepth = pos.z;
    float bias = max(0.004 * (dp + 1.0), 0.0005);
    vec2 shadow = vec2(0.0);
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for (int i = 0; i < 9; ++i) {
        float pcfDepth = texture(uShadowMap, pos.xy + pcfWeights[i].xy * texelSize).r;
        shadow += vec2(pointDepth - bias > pcfDepth? 0.0 : pcfWeights[i].z, pcfWeights[i].z);
    }
    return shadow.x / shadow.y;
}
#endif

void main(void) {
    if (uDebugMode == 1) {
        out_FragColor = vec4(GBuffer_FragDiffuse(), 1.0);
    }
    else if (uDebugMode == 2) {
        out_FragColor = vec4(GBuffer_FragEmissive(), 1.0);
    }
    else if (uDebugMode == 3) {
        out_FragColor = GBuffer_FragSpecGloss();
    }
    else if (uDebugMode == 4) {
        out_FragColor = texture(uGBufferNormal, vUV);
    }
    else if (uDebugMode == 5) {
        //out_FragColor = GBuffer_FragViewSpacePos();
        float depth = abs(GBuffer_FragViewSpacePos().z);
        //float depth = texelFetch(uGBufferDepth, ivec2(gl_FragCoord.xy), 0).r * 0.5 + 0.5;
        out_FragColor = vec4(vec3(depth * 0.3), 1.0);
        //out_FragColor = vec4(GBuffer_NormalizedFragCoord(), 0.0, 1.0);*/
    }
    else if (uDebugMode == 6) {
        out_FragColor = vec4(vec3(texture(uSSAOBuf, vUV).r), 1.0);
    }
    else if (uDebugMode == 7) {
        out_FragColor = vec4(vec3(texture(uShadowMap, vUV).s), 1.0);
    }
    else if (uDebugMode == 8) {
        out_FragColor = texture(uBloomFramebuf, vUV);
    }
    else if (uDebugMode == 9) {
        out_FragColor = texture(uBrightnessSamples, vUV) * 0.25;
    }
}
