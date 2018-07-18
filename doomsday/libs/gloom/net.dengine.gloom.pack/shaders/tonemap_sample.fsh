#version 330 core

#include "common/miplevel.glsl"

uniform sampler2D uFramebuf;
uniform float uCurrentFrameRate;

in vec2 vUV;

void main(void) {
    // Sample values are blended with colors from previous frames.
    float blendAlpha = 2.0 / uCurrentFrameRate;

    // Gather highly averaged colors for brightness analysis.
    // The rendering target is very small for these fragments.
    int mip = int(mipLevel(vUV, textureSize(uFramebuf, 0)));
    ivec2 levelSize = textureSize(uFramebuf, mip);
    vec2 texelSize = vec2(1.0 / vec2(levelSize));
    vec2 halfTexel = 0.5 * texelSize;
    out_FragColor.rgb = max(max(max(
                            textureLod(uFramebuf, vUV, mip).rgb,
                            textureLod(uFramebuf, vUV + vec2(halfTexel.x, 0.0), mip).rgb),
                            textureLod(uFramebuf, vUV + vec2(0.0, halfTexel.y), mip).rgb),
                            textureLod(uFramebuf, vUV + halfTexel, mip).rgb);
    out_FragColor.a = min(1.0, blendAlpha);
}
