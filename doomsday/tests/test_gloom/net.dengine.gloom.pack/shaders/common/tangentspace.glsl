/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Tangent space
 *
 * Copyright (c) 2015-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

 #ifndef GLOOM_TANGENTSPACE_H
 #define GLOOM_TANGENTSPACE_H

 struct TangentSpace {
     vec3 tangent;
     vec3 bitangent;
     vec3 normal;
 };

 mat3 Gloom_TangentMatrix(TangentSpace ts) {
     return mat3(ts.tangent, ts.bitangent, ts.normal);
 }

 #endif // GLOOM_TANGENTSPACE_H
