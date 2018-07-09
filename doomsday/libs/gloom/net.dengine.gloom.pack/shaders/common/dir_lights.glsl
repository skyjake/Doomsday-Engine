#ifndef GLOOM_DIR_LIGHTS_H
#define GLOOM_DIR_LIGHTS_H

uniform vec3    uLightIntensity;
uniform vec3    uViewSpaceLightOrigin;
uniform vec3    uViewSpaceLightDir;
uniform mat4    uLightMatrix; // world -> light
uniform mat4    uViewToLightMatrix;

uniform sampler2DShadow uShadowMap;

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
    // PCF.
    for (int i = 0; i < 9; ++i) {
        shadow += vec2(
            texture(uShadowMap, vec3(pos.xy + pcfWeights[i].xy * texelSize, pointDepth)),
            pcfWeights[i].z
        );
    }
    return shadow.x / shadow.y;
}

vec3 Gloom_DirectionalLighting(SurfacePoint sp) {
    vec3 outColor = vec3(0.0);
    float dirLight = 1.0;
    float dp = dot(sp.normal, uViewSpaceLightDir);
    if (dp < 0.0) {
        // Surface faces the light.
        vec4 lsPos = uViewToLightMatrix * vec4(sp.pos, 1.0);
        dirLight *= Gloom_FetchShadow(lsPos, dp);
        if (dirLight > 0.0) {
            Light light = Light(
                uViewSpaceLightOrigin,
                uViewSpaceLightDir,
                uLightIntensity,
                -1.0, 0.0 // no falloff
            );
            outColor += dirLight * Gloom_BlinnPhong(light, sp);
        }
    }
    return outColor;
}

#endif // GLOOM_DIR_LIGHTS_H
