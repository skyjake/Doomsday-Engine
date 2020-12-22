#version 330 core

#include "common/gbuffer_in.glsl"

const int   SAMPLE_COUNT = 64;
const float RADIUS = 0.5;
const int   NOISE_SIZE = 4;
const int   NOISE_MASK = 3;

uniform vec3          uSamples[SAMPLE_COUNT];
uniform samplerBuffer uNoise;
uniform mat4          uProjMatrix;

int noiseIndex(ivec2 pos) {
    pos &= NOISE_MASK;
    return pos.y * NOISE_SIZE + pos.x;
}

void main(void) {
    vec3 randomVec = texelFetch(uNoise, noiseIndex(ivec2(gl_FragCoord.xy))).rgb;
    vec3 normal    = GBuffer_FragViewSpaceNormal();
    vec3 fragPos   = GBuffer_FragViewSpacePos().xyz;

    if (normal == vec3(0.0)) {
        // If there is no normal, we won't sample for AO.
        out_FragColor = vec4(1.0, 0.0, 0.0, 0.0);
        return;
    }

    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tangentToView = mat3(tangent, bitangent, normal);

    float bias = 0.025;

    float occlusion = 0.0;
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        vec3 sample = tangentToView * uSamples[i];
        sample = fragPos + sample * RADIUS;

        // Project the sample.
        vec4 offset = vec4(sample, 1.0);
        offset = uProjMatrix * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sampleDepth = GBuffer_ViewSpacePos(offset.xy).z;
        float rangeCheck = smoothstep(0.0, 1.0, RADIUS / abs(fragPos.z - sampleDepth));
        occlusion += rangeCheck * (sampleDepth >= sample.z + bias? 1.0 : 0.0);
    }

    // Output:
    // - Red  : occlusion factor
    // - Green: view space distance (for depth-aware blurring)
    out_FragColor = vec4(1.0 - occlusion / SAMPLE_COUNT,
                         1.0 / -fragPos.z, 0.0, 0.0);
}
