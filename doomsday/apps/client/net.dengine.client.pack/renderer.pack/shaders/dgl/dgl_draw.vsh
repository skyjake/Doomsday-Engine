/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Legacy DGL Drawing
 *
 * Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#version 330

layout(location = 0) in vec4 aVertex;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord[3];

uniform mat4 uMvpMatrix;
uniform mat4 uTextureMatrix;

out vec4 vColor;
out vec2 vTexCoord[3];

vec2 transformTexCoord(vec2 tc) 
{
    vec4 coord = vec4(tc.s, tc.t, 0.0, 1.0);
    return (uTextureMatrix * coord).xy;
}

void main()
{
    gl_Position = uMvpMatrix * aVertex;
    vColor = aColor;    
    for (int i = 0; i < 3; ++i) {
        vTexCoord[i] = transformTexCoord(aTexCoord[i]);
    }
}
