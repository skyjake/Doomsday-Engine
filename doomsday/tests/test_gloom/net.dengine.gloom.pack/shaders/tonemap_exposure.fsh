#version 330 core

uniform sampler2D uFramebuf;
uniform float     uExposure;
//uniform sampler2D uDebugTex;
//uniform int       uDebugMode;

DENG_VAR vec2 vUV;

void main(void) {
    const float gamma = 0.5; //1.4;
    vec3 hdr = textureLod(uFramebuf, vUV, 0).rgb;
    // vec3 mapped = hdr * uExposure;
    vec3 mapped = 1.0 - exp(-hdr * uExposure);
    mapped = pow(mapped, 1.0 / vec3(gamma));
    out_FragColor = vec4(mapped, 1.0);
}
