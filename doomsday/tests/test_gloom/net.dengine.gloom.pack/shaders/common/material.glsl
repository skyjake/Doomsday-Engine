#ifndef GLOOM_MATERIAL_H
#define GLOOM_MATERIAL_H

#include "defs.glsl"
#include "miplevel.glsl"

uniform sampler2D uTextureAtlas[4];
uniform sampler2D uTextureMetrics;

struct Metrics {
    bool  isValid;
    vec4  uvRect;
    vec2  sizeInTexels;
    float texelsPerMeter;
    vec2  scale;
};

const vec4 Material_DefaultTextureValue[4] = vec4[4] (
    vec4(1.0), // diffuse
    vec4(0.0), // specular/gloss
    vec4(0.0), // emissive
    vec4(0.5, 0.5, 0.5, 1.0) // normal/displacement
);

Metrics Gloom_TextureMetrics(uint matIndex, int texture) {
    Metrics metrics;
    int x = texture * 2;
    vec3 texelSize = texelFetch(uTextureMetrics, ivec2(x + 1, matIndex), 0).xyz;
    metrics.sizeInTexels = texelSize.xy;
    // Not all textures are defined/present.
    if (metrics.sizeInTexels == vec2(0.0)) {
        metrics.isValid = false;
        return metrics;
    }
    metrics.uvRect = texelFetch(uTextureMetrics, ivec2(x, matIndex), 0);
    metrics.texelsPerMeter = texelSize.z;
    metrics.scale          = vec2(metrics.texelsPerMeter) / metrics.sizeInTexels;
    metrics.isValid = true;
    return metrics;
}

vec4 Gloom_FetchTexture(uint matIndex, int texture, vec2 uv) {
    Metrics metrics = Gloom_TextureMetrics(matIndex, texture);
    if (!metrics.isValid) {
        return Material_DefaultTextureValue[texture];
    }
    vec2 normUV  = uv * metrics.scale;
    vec2 atlasUV = metrics.uvRect.xy + fract(normUV) * metrics.uvRect.zw;
    return textureLod(uTextureAtlas[texture], atlasUV,
                      mipLevel(normUV, metrics.sizeInTexels.xy) - 0.5);
}

#endif // GLOOM_MATERIAL_H
