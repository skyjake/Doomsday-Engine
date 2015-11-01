/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Lighting
 *
 * Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

uniform highp float uEmission; // factor for emissive light

uniform highp vec4 uAmbientLight;
uniform highp vec4 uLightIntensities[4];        
uniform highp vec3 uLightDirs[4];
uniform highp vec3 uEyePos;

varying highp vec3 vLightDirs[4]; // tangent space
varying highp vec3 vEyeDir;       // tangent space

#ifdef DENG_VERTEX_SHADER

void calculateSurfaceLighting(highp mat3 surface)
{
    vLightDirs[0] = uLightDirs[0] * surface;
    vLightDirs[1] = uLightDirs[1] * surface;
    vLightDirs[2] = uLightDirs[2] * surface;
    vLightDirs[3] = uLightDirs[3] * surface;
}

void calculateEyeDirection(highp vec4 vertex, highp mat3 surface)
{
    vEyeDir = (uEyePos - vertex.xyz/vertex.w) * surface;    
}

#endif // DENG_VERTEX_SHADER

highp vec4 diffuseLightContrib(int index, highp vec3 normal) 
{
    if(uLightIntensities[index].a <= 0.001) 
    {
        return vec4(0.0); // too dim
    }
    highp float d = dot(normal, normalize(vLightDirs[index]));
    return max(d * uLightIntensities[index], vec4(0.0));
}

highp vec4 diffuseLight(highp vec3 normal)
{
    return (uAmbientLight + 
                      diffuseLightContrib(0, normal) + 
                      diffuseLightContrib(1, normal) + 
                      diffuseLightContrib(2, normal) + 
                      diffuseLightContrib(3, normal));
}

#ifdef DENG_HAVE_UTEX

highp vec4 specularLightContrib(highp vec2 specularUV, int index, highp vec3 normal) 
{
    if(uLightIntensities[index].a <= 0.001) 
    {
        return vec4(0.0); // too dim
    }

    // Is the surface facing the light direction?
    highp float facing = smoothstep(0.0, 0.2, dot(vLightDirs[index], normal));
    if(facing <= 0.0)
    {
        return vec4(0.0); // wrong way
    }
    
    highp vec3 reflected = reflect(-vLightDirs[index], normal);
    
    // Check the specular texture for parameters.
    highp vec4 specular = texture2D(uTex, specularUV);
    highp float shininess = max(1.0, specular.a * 7.0);
    
    highp float d =  
        facing * 
        dot(normalize(vEyeDir), reflected) * 
        shininess - max(0.0, shininess - 1.0);
    return max(0.0, d) * 
        uLightIntensities[index] * 
        vec4(specular.rgb * 2.0, max(max(specular.r, specular.g), specular.b));
}

highp vec4 specularLight(highp vec2 specularUV, highp vec3 normal)
{
    return specularLightContrib(specularUV, 0, normal) +
           specularLightContrib(specularUV, 1, normal) +
           specularLightContrib(specularUV, 2, normal) +
           specularLightContrib(specularUV, 3, normal);
}

highp vec4 emittedLight(highp vec2 emissiveUV)
{
    highp vec4 emission = uEmission * texture2D(uTex, emissiveUV);        
    emission.a = 0.0; // Does not contribute to alpha.
    return emission;
}

#endif // DENG_HAVE_UTEX
