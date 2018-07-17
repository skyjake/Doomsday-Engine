#version 330 core

uniform sampler2D uFramebuf;
uniform float uCurrentFrameRate;

in vec2 vUV;

void main(void) {
    // Gather highly averaged colors for brightness analysis.
    // The rendering target is very small for these fragments.
    // Sample values are blended with colors from previous frames.
    float alpha = 1.0 / uCurrentFrameRate; // TODO: Adjust based on FPS.
    out_FragColor.rgb = texture(uFramebuf, vUV, -1.5).rgb;
    out_FragColor.a = alpha;
}
