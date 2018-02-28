#version 330 core

#include "common/gbuffer_in.glsl"
#include "common/lightmodel.glsl"

//uniform samplerCubeShadow uShadowMaps[6];
uniform float uShadowFarPlanes[6]; // isn't vRadius enough?

flat DENG_VAR vec3  vOrigin;     // view space
flat DENG_VAR vec3  vDirection;  // view space
flat DENG_VAR vec3  vIntensity;
flat DENG_VAR float vRadius;
//flat DENG_VAR int   vShadowIndex;

void main(void) {
    vec3 pos      = GBuffer_FragViewSpacePos().xyz;
    vec3 normal   = GBuffer_FragViewSpaceNormal();
    vec3 lightVec = normalize(vOrigin - pos);

    if (dot(normal, lightVec) < 0.0) {
        out_FragColor = vec4(0.0);
        return; // Wrong direction.
    }

    float lit = 1.0;
#if 0
    // Check the shadow map.
    if (vShadowIndex >= 0) {
        vec3 ray = pos - vOrigin;
        lit = texture(uShadowMaps[0], vec4(normalize(ray), length(ray)));
    }
    if (lit <= 0.001) return;
#endif

    vec4 albedo = texelFetch(uGBufferAlbedo, ivec2(gl_FragCoord.xy), 0);
    vec4 specGloss = vec4(0.2);

    Light light = Light(vOrigin, vDirection, vIntensity, vRadius, 1.0);
    SurfacePoint surf = SurfacePoint(pos, normal, albedo.rgb, specGloss);

    out_FragColor = vec4(lit * Gloom_BlinnPhong(light, surf), 0.0);
}
