/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "clearvisual.h"
#include <QtOpenGL>

ClearVisual::ClearVisual(Flags flags, const de::Vector4f& color, Visual *parent)
    : Visual(parent), _color(color), _flags(flags)
{}

void ClearVisual::drawSelf(DrawingStage stage) const
{
    if(stage != BeforeChildren) return;

    if(_flags & ColorBuffer)
    {
        glClearColor(_color.x, _color.y, _color.z, _color.w);
    }

    glClear((_flags & ColorBuffer? GL_COLOR_BUFFER_BIT : 0) |
            (_flags & DepthBuffer? GL_DEPTH_BUFFER_BIT : 0));
}
