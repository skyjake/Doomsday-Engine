#include "common/gbuffer_in.glsl"
#include "common/lightmodel.glsl"

flat DENG_VAR vec3 vOrigin;     // view space
flat DENG_VAR vec3 vDirection;  // view space
flat DENG_VAR vec3 vIntensity;
flat DENG_VAR float vRadius;

void main(void) {
    vec3 pos    = GBuffer_FragViewSpacePos().xyz;
    vec3 normal = GBuffer_FragViewSpaceNormal();
    vec4 albedo = texelFetch(uGBufferAlbedo, ivec2(gl_FragCoord.xy), 0);
    vec4 specGloss = vec4(0.2);

    Light light = Light(vOrigin, vDirection, vIntensity, vRadius, 1.0);
    SurfacePoint surf = SurfacePoint(pos, normal, albedo.rgb, specGloss);

    out_FragColor = vec4(Gloom_BlinnPhong(light, surf), 0.0);
}
