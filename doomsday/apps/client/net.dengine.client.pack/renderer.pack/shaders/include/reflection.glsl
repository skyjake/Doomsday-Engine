/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Reflection Cube Map
 *
 * Requires:
 * - tangentspace.glsl
 * - lighting.glsl (eye direction)
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

#define DENG_HAVE_UREFLECTIONTEX

uniform samplerCube uReflectionTex;
uniform highp mat4 uReflectionMatrix;

/*
 * Given the tangent space @a surfaceNormal, determines the reflection 
 * direction in local space. The normal is transformed to local space using
 * the fragment's tangent space matrix. @a bias is the mipmap bias for the 
 * reflection.
 */
highp vec3 reflectedColorBiased(highp vec3 surfaceNormal, float bias)
{
    // The reflection cube exists in local space, so we will use vectors
    // relative to the object's origin.
    highp vec3 reflectedDir = 
        reflect(normalize(vRelativeEyePos), 
                fragmentTangentSpace() * surfaceNormal);
    
    reflectedDir = (uReflectionMatrix * vec4(reflectedDir, 0.0)).xyz;
        
    // Match world space directions.
    reflectedDir.y = -reflectedDir.y;
    return textureCube(uReflectionTex, reflectedDir, bias).rgb;
}

highp vec3 reflectedColor(highp vec3 surfaceNormal)
{
    return reflectedColorBiased(surfaceNormal, 0.0);
}

highp vec4 diffuseAndReflectedLight(highp vec4 diffuseFactor, highp vec2 diffuseUV,
                                    highp vec2 specularUV, highp vec3 surfaceNormal)
{
    highp vec4 specGloss = specularGloss(specularUV); // from lighting.glsl
    highp float mipBias = max(0.0, (0.9 - specGloss.a) * 6.0);

    // Reflection.
    highp vec3 reflection = reflectedColorBiased(surfaceNormal, mipBias);
    highp vec4 color = vec4(reflection.rgb * specGloss.rgb, 0.0);
    
    // Diffuse.
    highp vec4 diffuse = diffuseFactor * texture2D(uTex, diffuseUV);
    color += (diffuse * diffuseLight(surfaceNormal) * // from lighting.glsl
              vec4(1.0 - specGloss.rgb, 1.0));
    return color;
}
