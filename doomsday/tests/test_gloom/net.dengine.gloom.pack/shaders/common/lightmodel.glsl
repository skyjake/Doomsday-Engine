#ifndef GLOOM_LIGHTMODEL_H
#define GLOOM_LIGHTMODEL_H

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

#endif // GLOOM_LIGHTMODEL_H
