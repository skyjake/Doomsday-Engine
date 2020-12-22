/** @file windowtransform.h  Base class for window content transformation.
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

#ifndef LIBAPPFW_WINDOWTRANSFORM_H
#define LIBAPPFW_WINDOWTRANSFORM_H

#include "libgui.h"
#include <de/rectangle.h>
#include <de/vector.h>

namespace de {

class BaseWindow;

/**
 * Base class for window content transformation.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC WindowTransform
{
public:
    WindowTransform(BaseWindow &window);

    virtual ~WindowTransform();

    BaseWindow &window() const;

    /**
     * Called by the window when GL is ready.
     */
    virtual void glInit();

    virtual void glDeinit();

    /**
     * Determines how large the root widget should be for a particular canvas size.
     * @param physicalCanvasSize  Canvas size (pixels).
     * @return Logical size (UI units).
     */
    virtual Vec2ui logicalRootSize(const Vec2ui &physicalCanvasSize) const;

    /**
     * Translate a point in physical window coordinates to logical coordinates.
     * @param pos  Window coordinates (pixels).
     * @return Logical coordinates inside the root widget's area.
     */
    virtual Vec2f windowToLogicalCoords(const Vec2i &pos) const;

    /**
     * Translate a point in logical coordinates to window coordinates.
     * @param rect  Logical coordinates.
     * @return Window coordinates (pixels).
     */
    virtual Vec2f logicalToWindowCoords(const Vec2i &pos) const;

    Rectanglef logicalToWindowCoords(const Rectanglei &rect) const;

    /**
     * Applies the appropriate transformation state and tells the window to draw its
     * contents.
     */
    virtual void drawTransformed();

    DE_CAST_METHODS()

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_WINDOWTRANSFORM_H
