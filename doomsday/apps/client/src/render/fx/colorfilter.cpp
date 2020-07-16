/** @file colorfilter.cpp
 *
 * @todo Refactor: Color filters should be console-specific.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "render/fx/colorfilter.h"
#include "gl/gl_draw.h"
#include "api_gl.h"

#include <de/glinfo.h>
#include <de/logbuffer.h>

using namespace de;

static bool drawFilter = false;
static Vec4f filterColor;

#undef GL_SetFilter
DE_EXTERN_C void GL_SetFilter(dd_bool enabled)
{
    drawFilter = CPP_BOOL(enabled);
}

#undef GL_SetFilterColor
DE_EXTERN_C void GL_SetFilterColor(float r, float g, float b, float a)
{
    Vec4f newColorClamped(de::clamp(0.f, r, 1.f),
                             de::clamp(0.f, g, 1.f),
                             de::clamp(0.f, b, 1.f),
                             de::clamp(0.f, a, 1.f));

    if (filterColor != newColorClamped)
    {
        filterColor = newColorClamped;

        LOG_AS("GL_SetFilterColor");
        LOGDEV_GL_XVERBOSE("%s", filterColor.asText());
    }
}

namespace fx {

ColorFilter::ColorFilter(int console) : ConsoleEffect(console)
{}

void ColorFilter::draw()
{
    if (drawFilter && filterColor.w > 0)
    {
        const Rectanglei rect = viewRect();

        DGL_Color4f(filterColor.x, filterColor.y, filterColor.z, filterColor.w);
        DGL_Begin(DGL_QUADS);
            DGL_Vertex2f(rect.topLeft.x,      rect.topLeft.y);
            DGL_Vertex2f(rect.topRight().x,   rect.topRight().y);
            DGL_Vertex2f(rect.bottomRight.x,  rect.bottomRight.y);
            DGL_Vertex2f(rect.bottomLeft().x, rect.bottomLeft().y);
        DGL_End();
    }
}

} // namespace fx
