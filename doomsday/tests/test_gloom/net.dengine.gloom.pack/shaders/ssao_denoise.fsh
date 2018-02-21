uniform sampler2D uNoisyFactors;

void main() {
    ivec2 fragCoord = ivec2(gl_FragCoord.xy);
    // Depth reciprocals are available in the buffer.
    float mainDepth = 1.0 / texelFetch(uNoisyFactors, fragCoord, 0).g;
    vec2 result = vec2(0.0, 0.0);
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 occl_depth = texelFetch(uNoisyFactors, fragCoord + ivec2(x, y), 0).rg;
            // Don't blur over deep edges.
            float f = 1.0 - step(0.5, abs(mainDepth - 1.0 / occl_depth.g));
            result += f * vec2(occl_depth.r, 1.0);
        }
    }
    out_FragColor = vec4(result.r / result.g);
}
