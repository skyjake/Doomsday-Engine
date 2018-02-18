highp float fogDensity(highp vec3 worldPos, highp vec3 eyePos, highp float fogEnd)
{
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
highp vec4 fogColor(highp vec3 worldPos, highp vec3 eyePos, highp vec4 fogSpec)
{
    return vec4(fogSpec.rgb, fogDensity(worldPos, eyePos, fogSpec.a));
}

highp vec4 mixFog(highp vec4 color, highp vec4 fog)
{
    return vec4(mix(color.rgb, fog.rgb, fog.a), color.a);
}

