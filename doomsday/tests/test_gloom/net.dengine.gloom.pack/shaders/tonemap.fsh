uniform sampler2D uFramebuf;
uniform float     uExposure;

DENG_VAR vec2 vUV;

void main(void) {
    out_FragColor = uExposure * textureLod(uFramebuf, vUV, 0);
}
