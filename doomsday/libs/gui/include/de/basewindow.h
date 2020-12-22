/** @file basewindow.h  Abstract base class for application windows.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_BASEWINDOW_H
#define LIBAPPFW_BASEWINDOW_H

#include "libgui.h"
#include <de/vector.h>

#if !defined (DE_MOBILE)
#  include <de/persistentglwindow.h>
#  define LIBAPPFW_BASEWINDOW_SUPER de::PersistentGLWindow
#else
#  include <de/GLWindow>
#  define LIBAPPFW_BASEWINDOW_SUPER de::GLWindow
#endif

namespace de {

class GuiRootWidget;
class WindowTransform;

/**
 * Abstract base class for application windows.
 *
 * All windows have a Canvas where the contents of the window are drawn. Windows may
 * additionally specify a content transformation using a WindowTransform object, which
 * will override the built-in transformation. The built-in transformation specifies an
 * "identity" transformation that doesn't differ from the logical layout.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC BaseWindow : public LIBAPPFW_BASEWINDOW_SUPER
{
public:
    BaseWindow(const String &id);

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
    virtual Vec2f windowContentSize() const = 0;

    /**
     * Returns the window root widget.
     */
    virtual GuiRootWidget &root() = 0;

    void draw() override;

    /**
     * Causes the contents of the window to be drawn. The contents are drawn immediately
     * and the method does not return until everything has been drawn. The method should
     * draw an entire frame using the non-transformed logical size of the view.
     */
    virtual void drawWindowContent() = 0;

#if defined (DE_MOBILE)
    String configName(const String &key) const;
#endif

    /**
     * If the mouse has been moved, process an event with the mouse coordinates.
     */
    void processLatestMousePosition(bool evenIfUnchanged = false);

    Vec2i latestMousePosition() const;

protected:
    virtual void preDraw();
    virtual void postDraw();
    void rootUpdate() override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_BASEWINDOW_H
