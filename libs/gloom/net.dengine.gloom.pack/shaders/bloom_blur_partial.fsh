#version 330 core

#include "common/defs.glsl"
#include "common/exposure.glsl"

uniform int       uBloomMode;
uniform float     uMinValue;
uniform sampler2D uInputTex;
uniform int       uInputLevel;

const int   KERNEL_SIZE = 10;
const float BLOOM_GAIN  = 1.0;

const float weights[KERNEL_SIZE] = float[KERNEL_SIZE](
    0.104458908205,
    0.101034337854,
    0.091419649397,
    0.0773850625105,
    0.0612804235924,
    0.0453976543683,
    0.0314624185339,
    0.0203984810107,
    0.0123723041445,
    0.00702021448612
);

in vec2 vUV;

float brightness(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 brightColor(vec2 texCoord, float exposure) {
    vec3 color = exposure * textureLod(uInputTex, texCoord, uInputLevel).rgb;
    float bf = brightness(color) / uMinValue;
    if (bf > 1.0) {
        color = max(color * BLOOM_GAIN * min(1.0, bf - 1.0), 0.0);
    }
    else {
        color = vec3(0.0);
    }
    return color;
}

void main(void) {
    vec2 texel = 1.0 / vec2(textureSize(uInputTex, uInputLevel));
    if (uBloomMode == Bloom_Horizontal) {
        texel.y = 0.0;
    }
    else {
        texel.x = 0.0;
    }

    out_FragColor = vec4(0.0, 0.0, 0.0, 1.0);

    if (uMinValue == 0.0) {
        out_FragColor.rgb += weights[0] * texture(uInputTex, vUV).rgb;
        for (int i = 1; i < KERNEL_SIZE; ++i) {
            out_FragColor.rgb += weights[i] * texture(uInputTex, vUV + texel * i).rgb;
            out_FragColor.rgb += weights[i] * texture(uInputTex, vUV - texel * i).rgb;
        }
    }
    else {
        float exposure = Gloom_Exposure();
        out_FragColor.rgb += weights[0] * brightColor(vUV, exposure);
        for (int i = 1; i < KERNEL_SIZE; ++i) {
            out_FragColor.rgb += weights[i] * brightColor(vUV + texel * i, exposure);
            out_FragColor.rgb += weights[i] * brightColor(vUV - texel * i, exposure);
        }
    }
}
