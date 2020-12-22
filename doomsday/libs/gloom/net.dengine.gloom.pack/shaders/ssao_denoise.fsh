#version 330 core

uniform sampler2D uNoisyFactors;

//#define DE_SSAO_DISABLE_BLUR

void main() {
    ivec2 fragCoord = ivec2(gl_FragCoord.xy);
#if defined (DE_SSAO_DISABLE_BLUR)
    // Pass through the noisy factor values.
    out_FragColor = vec4(texelFetch(uNoisyFactors, fragCoord, 0).r);
#else
    // Depth reciprocals are available in the buffer.
    float mainDepth = 1.0 / texelFetch(uNoisyFactors, fragCoord, 0).g;
    if (mainDepth > 100.0) {
        // No need to filter.
        out_FragColor = vec4(texelFetch(uNoisyFactors, fragCoord, 0).r);
        return;
    }
    vec2 result = vec2(0.0, 0.0);
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 occl_depth = texelFetch(uNoisyFactors, fragCoord + ivec2(x, y), 0).rg;
            // Don't blur over deep edges.
            float f = 1.0 - step(0.1, abs(mainDepth - 1.0 / occl_depth.g));
            //float f = 1.0 - smoothstep(0.25, 1.0, abs(mainDepth - 1.0 / occl_depth.g));
            result += f * vec2(occl_depth.r, 1.0);
        }
    }
    out_FragColor = vec4(result.r / result.g);
#endif
}
