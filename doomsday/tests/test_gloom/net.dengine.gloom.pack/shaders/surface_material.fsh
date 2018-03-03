#version 330 core

#include "common/gbuffer_out.glsl"
#include "common/miplevel.glsl"

uniform sampler2D uDiffuseAtlas;
uniform sampler2D uTextureMetrics;

     DENG_VAR vec2  vUV;
     DENG_VAR vec3  vNormal;
flat DENG_VAR float vMaterial;
flat DENG_VAR uint  vFlags;

void main(void) {
    uint matIndex = uint(vMaterial + 0.5);

    // Albedo color.
    vec4  uvRect         = texelFetch(uTextureMetrics, ivec2(0, matIndex), 0);
    vec3  texelSize      = texelFetch(uTextureMetrics, ivec2(1, matIndex), 0).xyz;
    float texelsPerMeter = texelSize.z;
    vec2  texScale       = vec2(texelsPerMeter) / texelSize.xy;

    vec2 normUV = vUV * texScale;
    vec2 uv = uvRect.xy + fract(normUV) * uvRect.zw;

    vec4 color = textureLod(uDiffuseAtlas, uv, mipLevel(normUV, texelSize.xy) - 0.5);
    if (color.a < 0.005)
    {
        discard;
    }

    out_FragColor = color;
    GBuffer_SetFragNormal(vNormal);
}
