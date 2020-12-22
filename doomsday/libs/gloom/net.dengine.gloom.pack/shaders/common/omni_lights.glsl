#ifndef GLOOM_OMNI_LIGHTS_H
#define GLOOM_OMNI_LIGHTS_H

#include "omni_shadows.glsl"
#include "lightmodel.glsl"

struct OmniLight {
    vec3  origin; // view space
    vec3  intensity;
    float falloffRadius;
    int   shadowIndex;
};

uniform int       uOmniLightCount;
uniform OmniLight uOmniLights[Gloom_MaxOmniLights];

vec3 Gloom_OmniLighting(SurfacePoint sp) {
    vec3 outColor = vec3(0.0);
    for (int i = 0; i < uOmniLightCount; ++i) {
        Light light = Light(
            uOmniLights[i].origin,
            vec3(0.0),
            uOmniLights[i].intensity,
            uOmniLights[i].falloffRadius,
            1.0);
        float lit = Gloom_OmniShadow(sp.pos, light, uOmniLights[i].shadowIndex);
        if (lit > 0.0) {
            outColor += lit * Gloom_BlinnPhong(light, sp);
        }
    }
    return outColor;
}

#endif // GLOOM_OMNI_LIGHTS_H
