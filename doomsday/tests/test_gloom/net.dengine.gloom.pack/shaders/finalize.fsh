#include "common/gbuffer_in.glsl"

uniform sampler2D uSSAOBuf;
uniform sampler2D uShadowMap;
uniform mat4 uViewToLightMatrix;
uniform vec3 uViewSpaceLightDir;
uniform int uDebugMode;

DENG_VAR vec2 vUV;

float Gloom_FetchShadow(vec4 lightSpacePos, vec3 normal) {
    vec3 pos = (lightSpacePos.xyz / lightSpacePos.w) * 0.5 + 0.5;
    float closestDepth = texture(uShadowMap, pos.xy).r;
    float pointDepth = pos.z;
    //float bias = max(0.05 * (1.0 + dot(normal, uViewSpaceLightDir)), 0.005);
    float bias = 0.005;
    float shadow = (pointDepth - bias > closestDepth? 0.0 : 1.0);
    return shadow;
}

void main(void) {
    if (uDebugMode == 0) {
        vec3 normal = GBuffer_FragViewSpaceNormal();
        float f = 1.0;
        if (normal != vec3(0.0)) {
            float dp = dot(normal, uViewSpaceLightDir);
            float light = max(0.0, -dp);
            float shadow = step(0.0, -dp);
            if (dp < 0.0) {
                // Surface faces the light.
                vec4 lsPos = uViewToLightMatrix * GBuffer_FragViewSpacePos();
                shadow *= Gloom_FetchShadow(lsPos, normal);
            }
            f = 0.3 + shadow * light;
        }
        out_FragColor = f *
            texture(uGBufferAlbedo, vUV) *
            texture(uSSAOBuf, vUV).r;
    }
    else if (uDebugMode == 1) {
        out_FragColor = texture(uGBufferAlbedo, vUV);
    }
    else if (uDebugMode == 2) {
        out_FragColor = texture(uGBufferNormal, vUV);
    }
    else if (uDebugMode == 3) {
        out_FragColor = GBuffer_FragViewSpacePos();
    }
    else if (uDebugMode == 4) {
        out_FragColor = vec4(vec3(texture(uSSAOBuf, vUV).r), 1.0);
    }
    else if (uDebugMode == 5) {
        out_FragColor = vec4(vec3(texture(uShadowMap, vUV).s), 1.0);
    }
}
