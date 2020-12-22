#ifndef GLOOM_OMNI_SHADOWS_H
#define GLOOM_OMNI_SHADOWS_H

#include "camera.glsl"

const int Gloom_MaxOmniLights = 6;

uniform samplerCubeShadow uShadowMaps[Gloom_MaxOmniLights];

float Gloom_OmniShadow(vec3 viewSpaceSurfacePos, Light light, int shadowIndex) {
    float lit = 1.0;

    // Check the shadow map.
    if (shadowIndex >= 0) {
        vec3 vsRay = light.origin - viewSpaceSurfacePos;
        float len = length(vsRay);
        vec3 worldRay = uViewToWorldRotate * vsRay;
        len *= 0.98;

#if 0
        // Percentage-closer filtering.
        {
            //lit   1.0;
            lit = 0.0;
            float bias    = 0.95;
            float samples = 4.0;
            float offset  = 0.05;
            for (float x = -offset; x < offset; x += offset / (samples * 0.5))
            {
                for (float y = -offset; y < offset; y += offset / (samples * 0.5))
                {
                    for (float z = -offset; z < offset; z += offset / (samples * 0.5))
                    {
                        vec3 vec = worldRay + vec3(x, y, z);
                        float ref = (length(vec) * bias) / vRadius;
                        lit += texture(uShadowMaps[vShadowIndex], vec4(vec, ref));
                        //closestDepth *= far_plane;   // Undo mapping [0;1]
                        //if(currentDepth - bias > closestDepth)
                            //shadow += 1.0;
                    }
                }
            }
            lit /= (samples * samples * samples);
        }
#endif

        float ref = len / light.falloffRadius;
        vec4 shadowUV = vec4(worldRay, ref);

        switch (shadowIndex) {
            /* NOTE: OpenGL 4 is required for variable-value indexing. */
        case 0:
            lit = texture(uShadowMaps[0], shadowUV);
            break;
        case 1:
            lit = texture(uShadowMaps[1], shadowUV);
            break;
        case 2:
            lit = texture(uShadowMaps[2], shadowUV);
            break;
        case 3:
            lit = texture(uShadowMaps[3], shadowUV);
            break;
        case 4:
            lit = texture(uShadowMaps[4], shadowUV);
            break;
        case 5:
            lit = texture(uShadowMaps[5], shadowUV);
            break;
        }
    }

    return lit;
}

#endif // GLOOM_OMNI_SHADOWS_H
