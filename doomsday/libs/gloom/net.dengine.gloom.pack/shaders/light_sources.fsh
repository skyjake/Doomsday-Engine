#version 330 core

#include "common/gbuffer_in.glsl"
#include "common/camera.glsl"
#include "common/lightmodel.glsl"
#include "common/omni_shadows.glsl"

flat in vec3  vOrigin;     // view space
flat in vec3  vDirection;  // view space
flat in vec3  vIntensity;
flat in float vRadius;
flat in int   vShadowIndex;

void main(void) {
    Light light = Light(vOrigin, vDirection, vIntensity, vRadius, 1.0);
    vec3 pos    = GBuffer_FragViewSpacePos().xyz;
    float lit   = Gloom_OmniShadow(pos, light, vShadowIndex);

    if (lit <= 0.001) {
        discard;
    }

    vec3 normal    = GBuffer_FragViewSpaceNormal();
    vec3 diffuse   = GBuffer_FragDiffuse();
    vec4 specGloss = GBuffer_FragSpecGloss();

    SurfacePoint sp = SurfacePoint(pos, normal, diffuse, specGloss);

    // Radius is scaled: volume is not a perfect sphere, avoid reaching edges.
    // light.falloffRadius *= 0.95;

    out_FragColor = vec4(lit * Gloom_BlinnPhong(light, sp), 0.0); // blend: add
}
