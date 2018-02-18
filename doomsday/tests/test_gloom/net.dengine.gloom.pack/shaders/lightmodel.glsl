highp float lightIntensity(highp vec3 normal, highp vec3 lightDir)
{
    return clamp(dot(normal, -lightDir) + 0.1, 0.0, 1.0);
}

/**
 * Light model with one Sun-like bright light source at an infinite distance.
 * 
 * @param normal    Surface normal.
 * @param lightDir  Light direction vector.
 * @param fogSpec   Fog RGB, maxdistance.
 */
highp vec3 sunLightFactor(highp vec3 normal, highp vec3 lightDir, highp vec4 fogSpec)
{
    // Ambient light.
    highp vec3 ambientColor = fogSpec.rgb * 0.75;    

    highp float intensity = lightIntensity(normal, lightDir);
    
    return (ambientColor + vec3(intensity)) * 0.6;
}

