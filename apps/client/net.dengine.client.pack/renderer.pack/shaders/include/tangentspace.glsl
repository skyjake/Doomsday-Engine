/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Tangent space
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

#ifdef DE_VERTEX_SHADER

in vec3 aNormal;
in vec3 aTangent;
in vec3 aBitangent;

out vec3 vTSNormal;
out vec3 vTSTangent;
out vec3 vTSBitangent;

vec3 transformVector(vec3 dir, mat4 matrix) 
{
    return (matrix * vec4(dir, 0.0)).xyz;
}

void setTangentSpace(mat4 modelSpace)
{
    vTSNormal    = transformVector(aNormal,    modelSpace);
    vTSTangent   = transformVector(aTangent,   modelSpace);
    vTSBitangent = transformVector(aBitangent, modelSpace);
}

#endif // DE_VERTEX_SHADER

#ifdef DE_FRAGMENT_SHADER

in  vec3 vTSNormal;
in  vec3 vTSTangent;
in  vec3 vTSBitangent;

mat3 fragmentTangentSpace()
{
    return mat3(normalize(vTSTangent), normalize(vTSBitangent), normalize(vTSNormal));
}

vec3 modelSpaceNormalVector(vec2 uv)
{
    return fragmentTangentSpace() * normalVector(uv);
}

#endif // DE_FRAGMENT_SHADER
