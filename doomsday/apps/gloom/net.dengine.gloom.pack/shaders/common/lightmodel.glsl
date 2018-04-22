#ifndef GLOOM_LIGHTMODEL_H
#define GLOOM_LIGHTMODEL_H

#include "camera.glsl"
#include "defs.glsl"

uniform samplerCube uEnvMap;
uniform vec3 uEnvIntensity;

struct Light {
    vec3 origin;
    vec3 direction;
    vec3 intensity;
    float falloffRadius; // negative if no falloff
    float sourceRadius;
};

struct SurfacePoint {
    vec3 pos;
    vec3 normal;
    vec3 diffuse;
    vec4 specGloss;
};

vec3 Gloom_BlinnPhong(Light light, SurfacePoint surf) {
    float edgeFalloff = 1.0, falloff = 1.0;
    if (light.falloffRadius > 0.0) {
        float linear = max(light.sourceRadius, distance(surf.pos, light.origin));
        edgeFalloff  = clamp(2.0 - 2.0 * linear / light.falloffRadius, 0.0, 1.0);
        falloff      = clamp(1.0 / pow(linear, 2.0) * edgeFalloff, 0.0, 1.0);
    }

    vec3 lightDir   = normalize(light.origin - surf.pos);
    vec3 viewDir    = normalize(-surf.pos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    float diffuse = max(0.0, dot(surf.normal, lightDir));

    vec3 specular = vec3(0.0);
    if (surf.specGloss.rgb != vec3(0.0)) {
        float gloss = max(1.0, 1024.0 * surf.specGloss.a);
        // Blinn-Phong.
        float spec = abs(pow(max(dot(surf.normal, halfwayDir), 0.0), gloss));
        specular = surf.specGloss.rgb * light.intensity * spec * edgeFalloff;
    }
    return (light.intensity * falloff * diffuse * surf.diffuse) + specular;
}

vec3 Gloom_CubeReflection(samplerCube cube, SurfacePoint surf) {
    float mipBias = Material_MaxReflectionBlur * (1.0 - surf.specGloss.a);
    vec3 reflectedDir = reflect(normalize(surf.pos), surf.normal);
    return surf.specGloss.rgb * uEnvIntensity *
        texture(cube, uViewToWorldRotate * reflectedDir,
            min(mipBias, Material_MaxReflectionBias)).rgb;
}

#endif // GLOOM_LIGHTMODEL_H
