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

DE_LAYOUT_LOC(0) DE_ATTRIB vec4 aVertex;
DE_LAYOUT_LOC(1) DE_ATTRIB vec4 aColor;
DE_LAYOUT_LOC(2) DE_ATTRIB vec2 aTexCoord[2];
DE_LAYOUT_LOC(4) DE_ATTRIB vec2 aFragOffset;
DE_LAYOUT_LOC(5) DE_ATTRIB float aBatchIndex;

uniform vec2 uFragmentSize; // used for line width
uniform mat4 uMvpMatrix[DGL_BATCH_MAX];
uniform mat4 uTexMatrix0[DGL_BATCH_MAX];
uniform mat4 uTexMatrix1[DGL_BATCH_MAX];

flat DE_VAR int vBatchIndex;
DE_VAR vec4 vColor;
DE_VAR vec2 vTexCoord[2];

vec2 transformTexCoord(const mat4 matrix, const vec2 tc)
{
    vec4 coord = vec4(tc.s, tc.t, 0.0, 1.0);
    return (matrix * coord).xy;
}

void main()
{
    vBatchIndex = int(aBatchIndex + 0.5);

    gl_Position = uMvpMatrix[vBatchIndex] * aVertex;

    if (uFragmentSize != vec2(0.0))
    {
        gl_Position.xy +=
            normalize(mat2(uMvpMatrix[vBatchIndex]) * aFragOffset) * uFragmentSize * gl_Position.w;
    }

    vColor       = aColor;
    vTexCoord[0] = transformTexCoord(uTexMatrix0[vBatchIndex], aTexCoord[0]);
    vTexCoord[1] = transformTexCoord(uTexMatrix1[vBatchIndex], aTexCoord[1]);
}
