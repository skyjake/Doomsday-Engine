#version 330 core

uniform sampler2D uFramebuf;

in vec2 vUV;

void main(void) {
    // Gather highly averaged colors for brightness analysis.
    // The rendering target is very small for these fragments.
    out_FragColor = texture(uFramebuf, vUV);
}
