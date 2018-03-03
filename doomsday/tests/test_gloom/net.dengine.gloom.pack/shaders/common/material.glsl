#ifndef GLOOM_MATERIAL_H
#define GLOOM_MATERIAL_H

#include "defs.glsl"
#include "miplevel.glsl"

uniform sampler2D uTextureAtlas[4];
uniform sampler2D uTextureMetrics;

struct Metrics {
    vec4  uvRect;
    vec2  sizeInTexels;
    float texelsPerMeter;
    vec2  scale;
};

Metrics Gloom_TextureMetrics(uint matIndex, int texture) {
    Metrics metrics;
    int x = texture * 2;
    metrics.uvRect = texelFetch(uTextureMetrics, ivec2(x, matIndex), 0);
    vec3 texelSize = texelFetch(uTextureMetrics, ivec2(x + 1, matIndex), 0).xyz;
    metrics.sizeInTexels   = texelSize.xy;
    metrics.texelsPerMeter = texelSize.z;
    metrics.scale          = vec2(metrics.texelsPerMeter) / metrics.sizeInTexels;
    return metrics;
}

vec4 Gloom_FetchTexture(uint matIndex, int texture, vec2 uv) {
    Metrics metrics = Gloom_TextureMetrics(matIndex, texture);
    vec2 normUV  = uv * metrics.scale;
    vec2 atlasUV = metrics.uvRect.xy + fract(normUV) * metrics.uvRect.zw;
    return textureLod(uTextureAtlas[texture], atlasUV,
                      mipLevel(normUV, metrics.sizeInTexels.xy) - 0.5);
}

#endif // GLOOM_MATERIAL_H
