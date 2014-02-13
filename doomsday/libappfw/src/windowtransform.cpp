/** @file windowtransform.cpp  Base class for window content transformation.
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

#include "de/WindowTransform"
#include "de/BaseWindow"

namespace de {

DENG2_PIMPL_NOREF(WindowTransform)
{
    BaseWindow *win;
};

WindowTransform::WindowTransform(BaseWindow &window)
    : d(new Instance)
{
    d->win = &window;
}

BaseWindow &WindowTransform::window() const
{
    DENG2_ASSERT(d->win != 0);
    return *d->win;
}

void WindowTransform::glInit()
{
    // nothing to do
}

void WindowTransform::glDeinit()
{
    // nothing to do
}

Vector2ui WindowTransform::logicalRootSize(Vector2ui const &physicalCanvasSize) const
{
    return physicalCanvasSize;
}

Vector2f WindowTransform::windowToLogicalCoords(Vector2i const &pos) const
{
    return pos;
}

void WindowTransform::drawTransformed()
{
    return d->win->drawWindowContent();
}

} // namespace de
