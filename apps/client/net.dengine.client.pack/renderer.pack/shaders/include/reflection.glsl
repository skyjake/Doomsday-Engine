/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Reflection Cube Map
 *
 * Requires:
 * - tangentspace.glsl
 * - lighting.glsl (eye direction)
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

#define DE_HAVE_UREFLECTIONTEX

#define MAX_REFLECTION_BIAS 5.0

uniform samplerCube uReflectionTex;
uniform mat4 uReflectionMatrix;
uniform float uReflection;
uniform float uReflectionBlur;

/*
 * Given the tangent space @a surfaceNormal, determines the reflection 
 * direction in local space. The normal is transformed to local space using
 * the fragment's tangent space matrix. @a bias is the mipmap bias for the 
 * reflection.
 */
vec3 reflectedColorBiased(vec3 msNormal, float bias)
{
    // The reflection cube exists in local space, so we will use vectors
    // relative to the object's origin.
    vec3 reflectedDir = reflect(normalize(vEyeDir), msNormal);
    
    reflectedDir = (uReflectionMatrix * vec4(reflectedDir, 0.0)).xyz;
        
    // Match world space directions.
    reflectedDir.z = -reflectedDir.z;
    reflectedDir.y = -reflectedDir.y;
    return uReflection * texture(uReflectionTex, reflectedDir,
                                 min(bias, MAX_REFLECTION_BIAS)).rgb;
}

vec3 reflectedColor(vec3 msNormal)
{
    return reflectedColorBiased(msNormal, 0.0);
}

vec4 diffuseAndReflectedLight(vec4 diffuseFactor, vec2 diffuseUV,
                                    vec4 specGloss, vec3 msNormal)
{
    // Reflection.
    float mipBias = uReflectionBlur * (1.0 - specGloss.a);
    vec3 reflection = specGloss.rgb * reflectedColorBiased(msNormal, mipBias);
    
    // Diffuse. Surface opacity is also determined here.
    vec4 color = diffuseFactor * vec4(diffuseLight(msNormal), 1.0) * 
                       texture(uTex, diffuseUV);
    return color + vec4(reflection, 0.0);
}
