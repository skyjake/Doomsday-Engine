#version 330 core

#include "common/exposure.glsl"

uniform sampler2D uFramebuf;
uniform sampler2D uBloomFramebuf;

in vec2 vUV;

void main(void) {
    vec3 hdr = texelFetch(uFramebuf, ivec2(gl_FragCoord.xy), 0).rgb +
               texture(uBloomFramebuf, vUV).rgb;

    // const float gamma = 0.5;
    // vec3 mapped = hdr * uExposure;

    float exposure = Gloom_Exposure();

    const float gamma = 0.5; //1.4;
    vec3 mapped = 1.0 - exp(-hdr * exposure);

    mapped = pow(mapped, 1.0 / vec3(gamma));

    out_FragColor = vec4(mapped, 1.0);
}
