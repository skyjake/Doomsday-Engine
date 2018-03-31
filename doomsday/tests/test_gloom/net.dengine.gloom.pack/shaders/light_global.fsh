#version 330 core

#include "common/defs.glsl"
#include "common/gbuffer_in.glsl"
#include "common/camera.glsl"
#include "common/lightmodel.glsl"
#include "common/omni_lights.glsl"

uniform sampler2D uSSAOBuf;

// Directional lights:
uniform vec3    uLightIntensity;
uniform vec3    uViewSpaceLightOrigin;
uniform vec3    uViewSpaceLightDir;
uniform mat4    uLightMatrix; // world -> light
uniform mat4    uViewToLightMatrix;
uniform sampler2DShadow uShadowMap;

// Omni lights:
struct OmniLight {
    vec3  origin; // view space
    vec3  intensity;
    float falloffRadius;
    int   shadowIndex;
};
uniform int       uOmniLightCount;
uniform OmniLight uOmniLights[Gloom_MaxOmniLights];

in vec2 vUV;

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

vec3 Gloom_FetchAmbient(vec3 viewSpaceNormal) {
    return 0.8 * uEnvIntensity * textureLod(uEnvMap, vec3(0, 1, 0), 8).rgb;
}

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
    // PCF.
    for (int i = 0; i < 9; ++i) {
        shadow += vec2(
            texture(uShadowMap, vec3(pos.xy + pcfWeights[i].xy * texelSize, pointDepth)),
            pcfWeights[i].z
        );
    }
    return shadow.x / shadow.y;
}

void main(void) {
    vec4 vsPos     = GBuffer_FragViewSpacePos();
    vec3 diffuse   = GBuffer_FragDiffuse();
    vec4 specGloss = GBuffer_FragSpecGloss();
    vec3 normal    = GBuffer_FragViewSpaceNormal();

    // Ambient light.
    vec3 ambient = Gloom_FetchAmbient(normal);
    float ambientOcclusion = texture(uSSAOBuf, vUV).r;

    vec3 outColor = ambient * ambientOcclusion * diffuse;

    /* Directional world lights. */ {
        float dirLight = 1.0;
        float dp = dot(normal, uViewSpaceLightDir);
        if (dp < 0.0) {
            // Surface faces the light.
            vec4 lsPos = uViewToLightMatrix * vsPos;
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

    /* Reflection component. */ {
        float mipBias = Material_MaxReflectionBlur * (1.0 - specGloss.a);
        vec3 reflectedDir = reflect(normalize(vsPos.xyz), normal);
        outColor += specGloss.rgb * uEnvIntensity *
            texture(uEnvMap, uViewToWorldRotate * reflectedDir,
                min(mipBias, Material_MaxReflectionBias)).rgb;
    }

    /* Omni lights (that were determined to affect most pixels in the frame). */ {
        SurfacePoint sp = SurfacePoint(vsPos.xyz, normal, diffuse, specGloss);
        for (int i = 0; i < uOmniLightCount; ++i) {
            Light light = Light(
                uOmniLights[i].origin,
                vec3(0.0),
                uOmniLights[i].intensity,
                uOmniLights[i].falloffRadius,
                1.0);
            float lit = Gloom_OmniShadow(vsPos.xyz, light, uOmniLights[i].shadowIndex);
            if (lit > 0.0) {
                outColor += lit * Gloom_BlinnPhong(light, sp);
            }
        }
    }

    // Material emissive component.
    outColor += GBuffer_FragEmissive();

    // The final color.
    out_FragColor = vec4(outColor, 1.0);
}
