/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Skeletal animation (vertex shader)
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

uniform highp mat4 uBoneMatrices[64];

attribute highp vec4 aBoneIDs;
attribute highp vec4 aBoneWeights;

/*
 * Calculates the bone matrix for the current vertex. Bones and their weights
 * are determined by vertex attributes.
 */
highp mat4 vertexBoneTransform()
{
    return uBoneMatrices[int(aBoneIDs.x + 0.5)] * aBoneWeights.x +
           uBoneMatrices[int(aBoneIDs.y + 0.5)] * aBoneWeights.y +
           uBoneMatrices[int(aBoneIDs.z + 0.5)] * aBoneWeights.z +
           uBoneMatrices[int(aBoneIDs.w + 0.5)] * aBoneWeights.w;
}
