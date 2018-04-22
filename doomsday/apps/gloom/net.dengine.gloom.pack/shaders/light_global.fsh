#version 330 core

#include "common/defs.glsl"
#include "common/gbuffer_in.glsl"
#include "common/camera.glsl"
#include "common/lightmodel.glsl"
#include "common/ambient.glsl"
#include "common/dir_lights.glsl"
#include "common/omni_lights.glsl"

in vec2 vUV;

void main(void) {
    vec3 vsPos     = GBuffer_FragViewSpacePos().xyz;
    vec3 normal    = GBuffer_FragViewSpaceNormal();
    vec3 diffuse   = GBuffer_FragDiffuse();
    vec4 specGloss = GBuffer_FragSpecGloss();

    SurfacePoint sp = SurfacePoint(vsPos, normal, diffuse, specGloss);

    // Ambient light.
    vec3 outColor = Gloom_AmbientLight(sp, vUV);

    // Directional world lights.
    outColor += Gloom_DirectionalLighting(sp);

    // Omni lights (that were determined to affect most pixels in the frame).
    outColor += Gloom_OmniLighting(sp);

    // Reflection component.
    outColor += Gloom_CubeReflection(uEnvMap, sp);

    // Material emissive component.
    outColor += GBuffer_FragEmissive();

    // The final color.
    out_FragColor = vec4(outColor, 1.0);
}
