#include "common/gbuffer_in.glsl"

const int SAMPLE_COUNT = 64;
const float RADIUS = 0.5;

uniform vec3 uSamples[SAMPLE_COUNT];
uniform sampler2D uNoise;
uniform mat4 uProjMatrix;

void main(void) {
    vec3 randomVec = texelFetch(uNoise, ivec2(gl_FragCoord.xy) % 8, 0).rgb;
    vec3 normal    = GBuffer_FragmentViewSpaceNormal();
    vec3 fragPos   = GBuffer_FragmentViewSpacePos().xyz;

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

    vec4 albedo = texelFetch(uGBufferAlbedo, ivec2(gl_FragCoord.xy), 0);

    //albedo = vec4(vec3(0.95), 1.0); // testing with white

    out_FragColor = vec4(vec3(1.0) - occlusion / SAMPLE_COUNT, 1.0) * albedo;
}
