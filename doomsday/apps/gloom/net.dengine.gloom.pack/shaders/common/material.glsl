#ifndef GLOOM_MATERIAL_H
#define GLOOM_MATERIAL_H

#include "defs.glsl"
#include "miplevel.glsl"
#include "time.glsl"

uniform sampler2D     uTextureAtlas[4];
uniform samplerBuffer uTextureMetrics;

struct Metrics {
    bool  isValid;
    vec4  uvRect;
    vec2  sizeInTexels;
    float texelsPerMeter;
    vec2  scale;
    uint  flags;
};

const vec4 Material_DefaultTextureValue[4] = vec4[4] (
    vec4(1.0), // diffuse
    vec4(0.0), // specular/gloss
    vec4(0.0), // emissive
    vec4(0.5, 0.5, 1.0, 1.0) // normal/displacement
);

const int Material_TextureMetricsTexelsPerTexture = 2;
const int Material_TextureMetricsTexelsPerElement =
    Material_TextureMetricsTexelsPerTexture * 4;

Metrics Gloom_TextureMetrics(uint matIndex, int texture) {
    Metrics metrics;
    int bufPos = int(matIndex) * Material_TextureMetricsTexelsPerElement +
                 texture       * Material_TextureMetricsTexelsPerTexture;
    vec4 texelSize = texelFetch(uTextureMetrics, bufPos + 1);
    metrics.sizeInTexels = texelSize.xy;
    // Not all textures are defined/present.
    if (metrics.sizeInTexels == vec2(0.0)) {
        metrics.isValid = false;
        return metrics;
    }
    metrics.uvRect         = texelFetch(uTextureMetrics, bufPos);
    metrics.texelsPerMeter = texelSize.z;
    metrics.scale          = vec2(metrics.texelsPerMeter) / metrics.sizeInTexels;
    metrics.flags          = floatBitsToUint(texelSize.w);
    metrics.isValid        = true;
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

vec4 Gloom_MaterialTexel(const MaterialSampler sampler, vec2 uv) {
    vec2 normUV = uv * sampler.metrics.scale;
    if (testFlag(sampler.metrics.flags, Metrics_VerticalAspect)) {
        normUV.y /= 1.2;
    }
    vec2 atlasUV = sampler.metrics.uvRect.xy + fract(normUV) * sampler.metrics.uvRect.zw;
    float mip = mipLevel(normUV, sampler.metrics.sizeInTexels.xy) - 0.5;
    switch (sampler.texture) {
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

vec4 Gloom_SampleMaterial(const MaterialSampler sampler, vec2 uv) {
    uint animation = (sampler.metrics.flags & Metrics_AnimationMask);
    if (animation == 0u) {
        return Gloom_MaterialTexel(sampler, uv);
    }
    else if (animation == 1u) {
        // Water offsets.
        vec2 waterOff1 = vec2( 0.182, -0.3195) * uCurrentTime;
        vec2 waterOff2 = vec2(-0.203, 0.01423) * uCurrentTime;
        vec4 texel1 = Gloom_MaterialTexel(sampler, uv + waterOff1);
        vec4 texel2 = Gloom_MaterialTexel(sampler, uv + waterOff2);
        return (texel1 + texel2) * 0.5;
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

vec2 Gloom_SamplerParallax(MaterialSampler matSamp, vec2 texCoords, vec3 tsViewDir,
                           out float displacementDepth) {
    if (!matSamp.metrics.isValid) {
        // Parallax does not have effect.
        displacementDepth = 0.0;
        return texCoords;
    }

    const float heightScale = 0.15; // TODO: Fetch from metrics.

#if 0
    // Basic Parallax Mapping
    float height = 1.0 - Gloom_FetchTexture(matIndex, Texture_NormalDisplacement, texCoords).a;
    vec2 p = tsViewDir.xy / tsViewDir.z * (height * heightScale);
    return texCoords - p;
#endif

    vec2 curTexCoords = texCoords;
    float curDepthMapValue = 1.0 - Gloom_SampleMaterial(matSamp, curTexCoords).a;

    if (curDepthMapValue == 0.0) {
        displacementDepth = 0.0;
        return texCoords;
    }

    float vdp = abs(dot(Axis_Z, tsViewDir));

    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, vdp);

    float layerDepth = 1.0 / numLayers;
    float curLayerDepth = 0.0;
    vec2 P = tsViewDir.xy * heightScale / vdp;
    vec2 deltaTexCoords = P / numLayers;

    while (curLayerDepth < curDepthMapValue) {
        curTexCoords -= deltaTexCoords;
        curDepthMapValue = 1.0 - Gloom_SampleMaterial(matSamp, curTexCoords).a;
        curLayerDepth += layerDepth;
    }

    // Interpolate for more accurate collision point.
    vec2 prevTexCoords = curTexCoords + deltaTexCoords;
    float d1 = curDepthMapValue;
    float d2 = 1.0 - Gloom_SampleMaterial(matSamp, prevTexCoords).a;
    float afterDepth = d1 - curLayerDepth;
    float beforeDepth = d2 - curLayerDepth + layerDepth;
    float weight = afterDepth / (afterDepth - beforeDepth);

    displacementDepth = mix(d1, d2, weight) * heightScale;

    return mix(curTexCoords, prevTexCoords, weight);
}

vec2 Gloom_Parallax(uint matIndex, vec2 texCoords, vec3 tsViewDir,
                    out float displacementDepth) {
    return Gloom_SamplerParallax(
        Gloom_Sampler(matIndex, Texture_NormalDisplacement),
        texCoords, tsViewDir, displacementDepth);
}

#endif // GLOOM_MATERIAL_H
