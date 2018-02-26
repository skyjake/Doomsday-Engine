uniform sampler2D uFramebuf;

DENG_VAR vec2 vUV;

void main(void) {
    // Gather highly averaged colors for brightness analysis.
    ivec2 texSize = textureSize(uFramebuf, 0);
    int dim = max(texSize.x, texSize.y);
    //out_FragColor = textureLod(uFramebuf, vUV, log2(dim));
    out_FragColor = texture(uFramebuf, vUV);
    // out_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}
