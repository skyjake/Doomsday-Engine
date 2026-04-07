#ifndef GLOOM_FOG_H
#define GLOOM_FOG_H

float fogDensity(vec3 worldPos, vec3 eyePos, float fogEnd) {
    return clamp(log(distance(worldPos, eyePos) / fogEnd)/3.0 + 1.0, 0.0, 1.0);
}

/**
 * Determine fog color at a given world position. The alpha component of fog color
 * determines how thick the fog is at the point.
 *
 * @param worldPos  World coordinates.
 * @param eyePos    Eye in world coordinates.
 * @param fogSpec   (R, G, B, maxdist)
 */
vec4 fogColor(vec3 worldPos, vec3 eyePos, vec4 fogSpec) {
    return vec4(fogSpec.rgb, fogDensity(worldPos, eyePos, fogSpec.a));
}

vec4 mixFog(vec4 color, vec4 fog) {
    return vec4(mix(color.rgb, fog.rgb, fog.a), color.a);
}

#endif // GLOOM_FOG_H
