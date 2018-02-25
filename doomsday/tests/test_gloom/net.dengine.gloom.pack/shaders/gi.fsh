#include "common/gbuffer_in.glsl"

uniform sampler2D uSSAOBuf;

DENG_VAR vec2 vUV;

void main(void) {
    vec3 ambient = vec3(0.35, 0.4, 0.5);

    ambient *= texture(uSSAOBuf, vUV).r;

    vec3 outColor =
        ambient  *
        texture(uGBufferAlbedo, vUV).rgb;

    // Emissive component.
    outColor += texture(uGBufferEmissive, vUV).rgb;

    // The final color.
    out_FragColor = vec4(outColor, 1.0);
}
