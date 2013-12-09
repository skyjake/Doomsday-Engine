/** @file colorfilterconsoleeffect.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

using namespace de;

namespace fx {

ColorFilter::ColorFilter(int console) : ConsoleEffect(console)
{}

void ColorFilter::draw()
{
    /// @todo Color filter should be console-specific.

    // The colored filter.
    if(GL_FilterIsVisible())
    {
        GL_DrawFilter();
    }
}

} // namespace fx
