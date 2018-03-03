#version 330 core

#include "common/gbuffer_in.glsl"
#include "common/lightmodel.glsl"
#include "common/material.glsl"

uniform sampler2D uSSAOBuf;

// Directional lights:
uniform vec3      uLightIntensity;
uniform vec3      uViewSpaceLightOrigin;
uniform vec3      uViewSpaceLightDir;
uniform mat4      uLightMatrix; // world -> light
uniform mat4      uViewToLightMatrix;
uniform mat3      uViewToWorldRotate;
uniform sampler2DShadow uShadowMap;

DENG_VAR vec2 vUV;

const vec3 pcfWeights[9] = vec3[9] (
    vec3( 0,  0, 2.4),
    vec3(-1, -1, 1.0),
    vec3( 0, -1, 1.4),
    vec3( 1, -1, 1.0),
    vec3(-1,  0, 1.4),
    vec3( 1,  0, 1.4),
    vec3(-1,  1, 1.0),
    vec3( 0,  1, 1.4),
    vec3( 1,  1, 1.0)
);

/**
 * Checks the shadow map to see if a point is in shadow.
 *
 * @param lightSpacePos  Light space position.
 * @param dp  Dot product of normal * lightvec.
 *
 * @return 0.0 if the point is in shadow, 1.0 if in the light.
 * Fractional values are returned at the shadow edge.
 */
float Gloom_FetchShadow(vec4 lightSpacePos, float dp) {
    vec3 pos = (lightSpacePos.xyz / lightSpacePos.w) * 0.5 + 0.5;
    float bias = max(0.004 * (dp + 1.0), 0.0005);
    float pointDepth = pos.z - bias;
    vec2 shadow = vec2(0.0);
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for (int i = 0; i < 9; ++i) {
        shadow += vec2(
            texture(uShadowMap, vec3(pos.xy + pcfWeights[i].xy * texelSize, pointDepth)),
            pcfWeights[i].z
        );
    }
    return shadow.x / shadow.y;
}

vec3 Gloom_FetchAmbient(vec3 viewSpaceNormal) {
    /*vec3 ambient = vec3(0.0);
    float scale = 3.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            for (int z = -1; z <= 1; ++z) {
                vec3 offset = vec3(x, y, z) * scale;
                ambient += textureLod(uEnvMap, uViewToWorldRotate *
                    (offset + viewSpaceNormal), 6).rgb;
            }
        }
    }
    return ambient / 9.0;*/

    return uEnvIntensity * textureLod(uEnvMap, vec3(0, 1, 0), 8).rgb;
}

void main(void) {
    MaterialData data = GBuffer_FragMaterialData();
    vec3 diffuse = Gloom_FetchTexture(data.matIndex, Texture_Diffuse, data.uv).rgb;
    vec4 specGloss = Gloom_FetchTexture(data.matIndex, Texture_SpecularGloss, data.uv);
    vec3 normal = GBuffer_FragViewSpaceNormal();

    // Ambient light.
    vec3 ambient = Gloom_FetchAmbient(normal);
    float ambientOcclusion = texture(uSSAOBuf, vUV).r;

    vec3 outColor = ambient * ambientOcclusion * diffuse;

#if 1
    // Directional world lights.
    {
        float dirLight = 1.0;
        float dp = dot(normal, uViewSpaceLightDir);
        if (dp < 0.0) {
            // Surface faces the light.
            vec4 lsPos = uViewToLightMatrix * GBuffer_FragViewSpacePos();
            dirLight *= Gloom_FetchShadow(lsPos, dp);
            if (dirLight > 0.0) {
                Light light = Light(
                    uViewSpaceLightOrigin,
                    uViewSpaceLightDir,
                    uLightIntensity,
                    -1.0, 0.0 // no falloff
                );
                SurfacePoint sp = SurfacePoint(lsPos.xyz/lsPos.w, normal, diffuse, specGloss);
                outColor += dirLight * Gloom_BlinnPhong(light, sp);
            }
        }
    }
#endif

#if 1
    // Material emissive component.
    outColor += Gloom_FetchTexture(data.matIndex, Texture_Emissive, data.uv).rgb;
#endif

    // The final color.
    out_FragColor = vec4(outColor, 1.0);
}
