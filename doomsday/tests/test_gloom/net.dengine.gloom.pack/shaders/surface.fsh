#include 'miplevel.glsl'

#line 3

uniform sampler2D uTex;
uniform sampler2D uTextureMetrics;
uniform float     uTexelsPerMeter;

DENG_VAR vec2 vUV;
DENG_VAR vec3 vNormal;
flat DENG_VAR uint vTexture;

void main(void) {
    vec4 uvRect    = texelFetch(uTextureMetrics, ivec2(0, vTexture), 0);
    vec4 texelSize = texelFetch(uTextureMetrics, ivec2(1, vTexture), 0);
    float texScale = uTexelsPerMeter / texelSize.x;

    vec2 normUV = vUV * texScale;
    vec2 uv = uvRect.xy + fract(normUV) * uvRect.zw;

    vec4 color = textureLod(uTex, uv, mipLevel(normUV, texelSize.xy) - 0.5);
    if (color.a < 0.005) discard;
    
    out_FragColor = color;
}
