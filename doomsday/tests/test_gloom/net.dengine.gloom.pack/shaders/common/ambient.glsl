#ifndef GLOOM_AMBIENT_H
#define GLOOM_AMBIENT_H

#include "lightmodel.glsl"

uniform sampler2D uSSAOBuf;

vec3 Gloom_FetchAmbient(vec3 viewSpaceNormal) {
    return 0.8 * uEnvIntensity * textureLod(uEnvMap, vec3(0, 1, 0), 8).rgb;
}

vec3 Gloom_AmbientLight(SurfacePoint sp, vec2 screenUV) {
    vec3 ambient = Gloom_FetchAmbient(sp.normal);
    float ambientOcclusion = texture(uSSAOBuf, screenUV).r;
    return ambient * ambientOcclusion * sp.diffuse;
}

#endif // GLOOM_AMBIENT_H
