/** @file windowtransform.cpp  Base class for window content transformation.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/windowtransform.h"
#include "de/basewindow.h"

namespace de {

DE_PIMPL_NOREF(WindowTransform)
{
    BaseWindow *win;
};

WindowTransform::WindowTransform(BaseWindow &window)
    : d(new Impl)
{
    d->win = &window;
}

WindowTransform::~WindowTransform()
{}

BaseWindow &WindowTransform::window() const
{
    DE_ASSERT(d->win != nullptr);
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

Vec2ui WindowTransform::logicalRootSize(const Vec2ui &physicalCanvasSize) const
{
    return physicalCanvasSize;
}

Vec2f WindowTransform::windowToLogicalCoords(const Vec2i &pos) const
{
    return pos;
}

Vec2f WindowTransform::logicalToWindowCoords(const Vec2i &pos) const
{
    return pos;
}

Rectanglef WindowTransform::logicalToWindowCoords(const Rectanglei &rect) const
{
    return Rectanglef(logicalToWindowCoords(rect.topLeft),
                      logicalToWindowCoords(rect.bottomRight));
}

void WindowTransform::drawTransformed()
{
    return d->win->drawWindowContent();
}

} // namespace de
