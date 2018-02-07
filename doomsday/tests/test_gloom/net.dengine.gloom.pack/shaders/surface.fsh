#include 'miplevel.glsl'

#line 3

uniform sampler2D uTex;
uniform sampler2D uTextureMetrics;

DENG_VAR highp vec2 vUV;
DENG_VAR highp vec3 vNormal;
flat DENG_VAR uint vTexture;

void main(void) {
    highp vec4 uvRect    = texelFetch(uTextureMetrics, ivec2(0, vTexture), 0);
    highp vec4 texelSize = texelFetch(uTextureMetrics, ivec2(1, vTexture), 0);

    highp vec2 uv = uvRect.xy + fract(vUV) * uvRect.zw;
    highp vec4 color = textureLod(uTex, uv, mipLevel(vUV, texelSize.xy) - 0.5);
    if (color.a < 0.005) discard;
    out_FragColor = color;
}
