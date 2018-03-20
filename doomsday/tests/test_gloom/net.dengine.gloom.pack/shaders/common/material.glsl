#ifndef GLOOM_MATERIAL_H
#define GLOOM_MATERIAL_H

#include "defs.glsl"
#include "miplevel.glsl"

uniform sampler2D     uTextureAtlas[4];
uniform samplerBuffer uTextureMetrics;

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

const int Material_TextureMetricsTexelsPerTexture = 2;
const int Material_TextureMetricsTexelsPerElement =
    Material_TextureMetricsTexelsPerTexture * 4;

Metrics Gloom_TextureMetrics(uint matIndex, int texture) {
    Metrics metrics;
    int bufPos = int(matIndex) * Material_TextureMetricsTexelsPerElement +
                 texture       * Material_TextureMetricsTexelsPerTexture;
    vec3 texelSize = texelFetch(uTextureMetrics, bufPos + 1).xyz;
    metrics.sizeInTexels = texelSize.xy;
    // Not all textures are defined/present.
    if (metrics.sizeInTexels == vec2(0.0)) {
        metrics.isValid = false;
        return metrics;
    }
    metrics.uvRect = texelFetch(uTextureMetrics, bufPos);
    metrics.texelsPerMeter = texelSize.z;
    metrics.scale          = vec2(metrics.texelsPerMeter) / metrics.sizeInTexels;
    metrics.isValid = true;
    return metrics;
}

struct MaterialSampler {
    uint matIndex;
    int texture;
    Metrics metrics;
};

MaterialSampler Gloom_Sampler(uint matIndex, int texture) {
    return MaterialSampler(
        matIndex,
        texture,
        Gloom_TextureMetrics(matIndex, texture)
    );
}

vec4 Gloom_SampleMaterial(const MaterialSampler sampler, vec2 uv) {
    vec2 normUV  = uv * sampler.metrics.scale;
    vec2 atlasUV = sampler.metrics.uvRect.xy + fract(normUV) * sampler.metrics.uvRect.zw;
    float mip = mipLevel(normUV, sampler.metrics.sizeInTexels.xy) - 0.5;
    switch (sampler.texture)
    {
    case Texture_Diffuse:
        return textureLod(uTextureAtlas[Texture_Diffuse], atlasUV, mip);
    case Texture_Emissive:
        return textureLod(uTextureAtlas[Texture_Emissive], atlasUV, mip);
    case Texture_SpecularGloss:
        return textureLod(uTextureAtlas[Texture_SpecularGloss], atlasUV, mip);
    case Texture_NormalDisplacement:
        return textureLod(uTextureAtlas[Texture_NormalDisplacement], atlasUV, mip);
    }
    return vec4(0.0);
}

vec4 Gloom_TryFetchTexture(uint matIndex, int texture, vec2 uv, vec4 fallback) {
    MaterialSampler ms = Gloom_Sampler(matIndex, texture);
    if (!ms.metrics.isValid) {
        return fallback;
    }
    return Gloom_SampleMaterial(ms, uv);
}

vec4 Gloom_FetchTexture(uint matIndex, int texture, vec2 uv) {
    return Gloom_TryFetchTexture(matIndex, texture, uv,
        Material_DefaultTextureValue[texture]);
}

#endif // GLOOM_MATERIAL_H
