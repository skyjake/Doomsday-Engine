uniform sampler2D uFramebuf;
uniform float     uExposure;

DENG_VAR vec2 vUV;

void main(void) {
    float gamma = 1.0; //1.4;
    //out_FragColor = uExposure * textureLod(uFramebuf, vUV, 0);
    vec3 hdr = textureLod(uFramebuf, vUV, 0).rgb;
    vec3 mapped = hdr * uExposure; //mapped; //1.0 - exp(-hdr * uExposure);
    mapped = pow(mapped, 1.0/vec3(gamma));
    out_FragColor = vec4(mapped, 1.0);
}
