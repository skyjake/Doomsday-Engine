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

uniform float uSpecular; // factor for specular lights
uniform float uEmission; // factor for emissive light
uniform float uGlossiness;

uniform vec4 uAmbientLight;
uniform vec4 uLightIntensities[4]; // colored
uniform vec3 uLightDirs[4]; // model space
uniform vec3 uEyePos; // model space

#ifdef DE_VERTEX_SHADER
out vec3 vEyeDir; // from vertex, in model space
#else
in  vec3 vEyeDir;
#endif

#ifdef DE_VERTEX_SHADER

void calculateEyeDirection(vec4 vertex) 
{
    vEyeDir = uEyePos - vertex.xyz/vertex.w;
}

#endif // DE_VERTEX_SHADER

vec3 diffuseLightContrib(int index, vec3 msNormal) 
{
    if (uLightIntensities[index].a <= 0.001) {
        return vec3(0.0); // too dim
    }
    float d = dot(msNormal, uLightDirs[index]);
    return max(d, 0.0) * uLightIntensities[index].rgb;
}

vec3 diffuseLight(vec3 msNormal)
{
    return (uAmbientLight.rgb + 
            diffuseLightContrib(0, msNormal) + 
            diffuseLightContrib(1, msNormal) + 
            diffuseLightContrib(2, msNormal) + 
            diffuseLightContrib(3, msNormal));
}

#ifdef DE_HAVE_UTEX

vec3 specularLightContrib(vec4 specGloss, int index, vec3 msNormal) 
{
    if (uLightIntensities[index].a <= 0.001) { // a == max of rgb
        return vec3(0.0); // Too dim.
    }
    vec3 reflectedDir = reflect(-uLightDirs[index], msNormal);
    float refDot = dot(normalize(vEyeDir), reflectedDir);
    if (refDot <= 0.0) {
        return vec3(0.0); // Wrong way.
    }
    float gloss = max(1.0, uGlossiness * specGloss.a);
    float specPower = min(pow(refDot, gloss), 1.0);
    return uSpecular * uLightIntensities[index].rgb * specPower * specGloss.rgb;
}

vec4 specularGloss(vec2 specularUV)
{
    return texture(uTex, specularUV);
}

vec3 specularLight(vec4 specGloss, vec3 msNormal)
{
    return specularLightContrib(specGloss, 0, msNormal) +
           specularLightContrib(specGloss, 1, msNormal) +
           specularLightContrib(specGloss, 2, msNormal) +
           specularLightContrib(specGloss, 3, msNormal);
}

vec4 emittedLight(vec2 emissiveUV)
{
    vec4 emission = uEmission * texture(uTex, emissiveUV);
    emission.a = 0.0; // Does not contribute to alpha.
    return emission;
}

#endif // DE_HAVE_UTEX
