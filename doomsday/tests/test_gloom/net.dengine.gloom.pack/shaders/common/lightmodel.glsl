#ifndef GLOOM_LIGHTMODEL_H
#define GLOOM_LIGHTMODEL_H

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
    vec3 albedo;
    vec4 specGloss;
};

vec3 Gloom_BlinnPhong(Light light, SurfacePoint surf) {
    float edgeFalloff = 1.0, falloff = 1.0;
    if (light.falloffRadius > 0.0) {
        float linear = max(light.sourceRadius, distance(surf.pos, light.origin));
        edgeFalloff  = clamp(2.0 - 2.0 * linear / light.falloffRadius, 0.0, 1.0);
        falloff      = clamp(1.0 / pow(linear, 2.0) * edgeFalloff, 0.0, 1.0);
    }
    vec3 lightDir     = normalize(light.origin - surf.pos);
    vec3 viewDir      = normalize(-surf.pos);
    vec3 halfwayDir   = normalize(lightDir + viewDir);

    // Blinn-Phong
    float shininess = 1024.0; // TODO: <= specGloss.a
    float spec = abs(pow(max(dot(surf.normal, halfwayDir), 0.0), shininess));
    vec3 specular = surf.specGloss.rgb * light.intensity * spec * edgeFalloff;
    float diffuse = max(0.0, dot(surf.normal, lightDir));

    return (light.intensity * falloff * diffuse * surf.albedo.rgb) + specular;
}

#if 0
float lightIntensity(vec3 normal, vec3 lightDir) {
    return clamp(dot(normal, -lightDir) + 0.1, 0.0, 1.0);
}

/**
 * Light model with one Sun-like bright light source at an infinite distance.
 *
 * @param normal    Surface normal.
 * @param lightDir  Light direction vector.
 * @param fogSpec   Fog RGB, maxdistance.
 */
vec3 sunLightFactor(vec3 normal, vec3 lightDir, vec4 fogSpec) {
    // Ambient light.
    vec3 ambientColor = fogSpec.rgb * 0.75;

    float intensity = lightIntensity(normal, lightDir);

    return (ambientColor + vec3(intensity)) * 0.6;
}
#endif

#endif // GLOOM_LIGHTMODEL_H
