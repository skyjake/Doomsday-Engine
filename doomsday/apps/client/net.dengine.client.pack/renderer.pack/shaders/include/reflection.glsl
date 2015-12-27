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
 * the fragment's tangent space matrix.
 */
highp vec3 reflectedColor(highp vec3 surfaceNormal)
{
    // The reflection cube exists in local space, so we will use vectors
    // relative to the object's origin.
    highp vec3 reflectedDir = 
        reflect(normalize(vRelativeEyePos), 
                fragmentTangentSpace() * surfaceNormal);
    
    reflectedDir = (uReflectionMatrix * vec4(reflectedDir, 0.0)).xyz;
        
    // Match world space directions.
    reflectedDir.y = -reflectedDir.y;
    return textureCube(uReflectionTex, reflectedDir).rgb;
}
