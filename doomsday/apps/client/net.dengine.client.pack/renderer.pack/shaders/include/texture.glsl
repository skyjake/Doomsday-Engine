/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Texture manipulation
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

#define DE_HAVE_UTEX

uniform sampler2D uTex; // texture atlas

/*
 * Maps the normalized @a uv to the rectangle defined by @a bounds.
 */
vec2 mapToBounds(vec2 uv, vec4 bounds)
{
    return bounds.xy + uv * bounds.zw;
}

vec3 normalVector(vec2 uv) 
{
    return normalize((texture(uTex, uv).xyz * 2.0) - 1.0);
}
