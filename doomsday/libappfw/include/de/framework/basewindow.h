/** @file basewindow.h  Abstract base class for application windows.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_BASEWINDOW_H
#define LIBAPPFW_BASEWINDOW_H

#include "../libappfw.h"

#include <de/Canvas>
#include <de/Vector>

namespace de {

class WindowTransform;

/**
 * Abstract base class for application windows.
 *
 * All windows must have a Canvas where the contents of the window are drawn. Windows may
 * additionally specify a content transformation using a WindowTransform object, which
 * will override the built-in transformation. The built-in transformation specifies an
 * "identity" transformation that doesn't differ from the logical layout.
 */
class BaseWindow
{
public:
    BaseWindow();

    /**
     * Sets a new content transformation being applied in the window. The provided
     * object must remain in existence as long as the BaseWindow instance uses it.
     *
     * @param xf  Content transformaton.
     */
    void setTransform(WindowTransform &xf);

    /**
     * Changes the window transformation to the default one that applies no actual
     * transformation.
     */
    void useDefaultTransform();

    /**
     * Returns the current content transformation being applied to the content of the
     * window.
     */
    WindowTransform &transform();

    /**
     * Returns the logical size of the window contents (e.g., root widget).
     */
    virtual Vector2f windowContentSize() = 0;

    /**
     * Returns the Canvas that represents the visible contents of the window.
     */
    virtual Canvas& windowCanvas() = 0;

    /**
     * Causes the contents of the window to be drawn. The contents are drawn immediately
     * and the method does not return until everything has been drawn. The method should
     * draw an entire frame using the non-transformed logical size of the view.
     */
    virtual void drawWindowContent() = 0;

    DENG2_AS_IS_METHODS()

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_BASEWINDOW_H
