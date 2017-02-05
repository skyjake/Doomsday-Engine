/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Lighting
 *
 * Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

uniform highp float uSpecular; // factor for specular lights
uniform highp float uEmission; // factor for emissive light
uniform highp float uGlossiness;

uniform highp vec4 uAmbientLight;
uniform highp vec4 uLightIntensities[4]; // colored
uniform highp vec3 uLightDirs[4]; // model space
uniform highp vec3 uEyePos; // model space

varying highp vec3 vEyeDir; // from vertex, in model space

#ifdef DENG_VERTEX_SHADER

void calculateEyeDirection(highp vec4 vertex) 
{
    vEyeDir = uEyePos - vertex.xyz/vertex.w;
}

#endif // DENG_VERTEX_SHADER

highp vec3 diffuseLightContrib(int index, highp vec3 msNormal) 
{
    if(uLightIntensities[index].a <= 0.001) 
    {
        return vec3(0.0); // too dim
    }
    highp float d = dot(msNormal, uLightDirs[index]);
    return max(d, 0.0) * uLightIntensities[index].rgb;
}

highp vec3 diffuseLight(highp vec3 msNormal)
{
    return (uAmbientLight.rgb + 
            diffuseLightContrib(0, msNormal) + 
            diffuseLightContrib(1, msNormal) + 
            diffuseLightContrib(2, msNormal) + 
            diffuseLightContrib(3, msNormal));
}

#ifdef DENG_HAVE_UTEX

highp vec3 specularLightContrib(highp vec4 specGloss, int index, highp vec3 msNormal) 
{
    if(uLightIntensities[index].a <= 0.001) // a == max of rgb
    {
        return vec3(0.0); // Too dim.
    }
    highp vec3 reflectedDir = reflect(-uLightDirs[index], msNormal);
    highp float refDot = dot(normalize(vEyeDir), reflectedDir);
    if(refDot <= 0.0)
    {        
        return vec3(0.0); // Wrong way.
    }
    highp float gloss = max(1.0, uGlossiness * specGloss.a);
    highp float specPower = min(pow(refDot, gloss), 1.0);
    return uSpecular * uLightIntensities[index].rgb * specPower * specGloss.rgb;
}

highp vec4 specularGloss(highp vec2 specularUV)
{
    return texture2D(uTex, specularUV);
}

highp vec3 specularLight(highp vec4 specGloss, highp vec3 msNormal)
{
    return specularLightContrib(specGloss, 0, msNormal) +
           specularLightContrib(specGloss, 1, msNormal) +
           specularLightContrib(specGloss, 2, msNormal) +
           specularLightContrib(specGloss, 3, msNormal);
}

highp vec4 emittedLight(highp vec2 emissiveUV)
{
    highp vec4 emission = uEmission * texture2D(uTex, emissiveUV);
    emission.a = 0.0; // Does not contribute to alpha.
    return emission;
}

#endif // DENG_HAVE_UTEX
