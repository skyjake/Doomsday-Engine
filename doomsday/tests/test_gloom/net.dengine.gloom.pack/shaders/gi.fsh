#include "common/gbuffer_in.glsl"
#include "common/lightmodel.glsl"

uniform sampler2D uSSAOBuf;

// Directional lights:
uniform vec3      uLightIntensity;
uniform vec3      uViewSpaceLightOrigin;
uniform vec3      uViewSpaceLightDir;
uniform mat4      uLightMatrix; // world -> light
uniform mat4      uViewToLightMatrix;
uniform sampler2D uShadowMap;

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
    float pointDepth = pos.z;
    float bias = max(0.004 * (dp + 1.0), 0.0005);
    vec2 shadow = vec2(0.0);
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for (int i = 0; i < 9; ++i) {
        float pcfDepth = texture(uShadowMap, pos.xy + pcfWeights[i].xy * texelSize).r;
        shadow += vec2(pointDepth - bias > pcfDepth? 0.0 : pcfWeights[i].z, pcfWeights[i].z);
    }
    return shadow.x / shadow.y;
}

void main(void) {
    vec3 albedo = texture(uGBufferAlbedo, vUV).rgb;
    vec4 specGloss = vec4(0.2); // TODO: from material
    vec3 normal = GBuffer_FragViewSpaceNormal();
    float ambientOcclusion = texture(uSSAOBuf, vUV).r;

    vec3 ambient = vec3(0.75, 0.8, 1.25);
    ambient *= ambientOcclusion;

    vec3 outColor = ambient * albedo;

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
                SurfacePoint sp = SurfacePoint(lsPos.xyz/lsPos.w, normal, albedo, specGloss);
                outColor += dirLight * Gloom_BlinnPhong(light, sp);
            }
        }
    }

    // Emissive component.
    outColor += texture(uGBufferEmissive, vUV).rgb;

    // The final color.
    out_FragColor = vec4(outColor, 1.0);
}
