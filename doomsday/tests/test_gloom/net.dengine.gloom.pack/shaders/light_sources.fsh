#version 330 core

#include "common/gbuffer_in.glsl"
#include "common/camera.glsl"
#include "common/lightmodel.glsl"

uniform samplerCubeShadow uShadowMaps[6];
// uniform samplerCube uShadowMaps[6];

//uniform float uShadowFarPlanes[6]; // isn't vRadius enough?

flat DENG_VAR vec3  vOrigin;     // view space
flat DENG_VAR vec3  vDirection;  // view space
flat DENG_VAR vec3  vIntensity;
flat DENG_VAR float vRadius;
flat DENG_VAR int   vShadowIndex;

void main(void) {
    vec3 pos      = GBuffer_FragViewSpacePos().xyz;
    vec3 normal   = GBuffer_FragViewSpaceNormal();

    out_FragColor = vec4(0.0); // By default no color is received.

    /*if (dot(normal, lightVec) < 0.0) {
        return; // Surface faces away from light.
    }*/

    float lit = 1.0;

    // Check the shadow map.
    if (vShadowIndex >= 0) {
        vec3 vsRay = vOrigin - pos;
        float len = length(vsRay);
        //vec3 lightVec = vsRay / len;
        /*lit = texture(uShadowMaps[vShadowIndex], vec4(uViewToWorldRotate * vsRay, len/25.0));*/
        vec3 worldRay = uViewToWorldRotate * vsRay;
        //float bias = 0.99 - max(0.05 * (dot(-normalize(vsRay), normal) + 1.0), 0.005);
        //len -= bias;
        //len *= 0.99;
        len *= 0.98;
        //len *= bias;
        // lit = (vRadius * texture(uShadowMaps[vShadowIndex], worldRay).r)
            // >= len? 1.0 : 0.0;

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

        float ref = len / vRadius;
        vec4 shadowUV = vec4(worldRay, ref);
        switch (vShadowIndex) {
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
        //out_FragColor = vec4(vec3(lit), 1.0);
        //return;
    }
    if (lit <= 0.001) {
        return;
    }

    vec3 diffuse   = GBuffer_FragDiffuse();
    vec4 specGloss = GBuffer_FragSpecGloss();

    // Radius is scaled: volume is not a perfect sphere, avoid reaching edges.
    Light light = Light(vOrigin, vDirection, vIntensity, vRadius * 0.95, 1.0);

    SurfacePoint surf = SurfacePoint(pos, normal, diffuse, specGloss);

    out_FragColor = vec4(lit * Gloom_BlinnPhong(light, surf), 0.0);
}
