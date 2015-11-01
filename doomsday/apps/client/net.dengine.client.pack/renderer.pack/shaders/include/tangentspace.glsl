/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Tangent space
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

attribute highp vec3 aNormal;
attribute highp vec3 aTangent;
attribute highp vec3 aBitangent;

highp vec3 transformVector(highp vec3 dir, highp mat4 matrix) 
{
    return (matrix * vec4(dir, 0.0)).xyz;
}

highp mat3 tangentSpace(highp mat4 modelSpace)
{
    highp vec3 normal    = transformVector(aNormal, modelSpace);
    highp vec3 tangent   = transformVector(aTangent, modelSpace);
    highp vec3 bitangent = transformVector(aBitangent, modelSpace);
    return mat3(tangent, bitangent, normal);    
}
