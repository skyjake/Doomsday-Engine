/** @file compositorwidget.h  Off-screen compositor.
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

#ifndef LIBAPPFW_COMPOSITORWIDGET_H
#define LIBAPPFW_COMPOSITORWIDGET_H

#include "de/guiwidget.h"

namespace de {

/**
 * Off-screen compositor.
 *
 * All children of the compositor are drawn to an off-screen render target
 * whose size is equal to the default render target. The default behavior of
 * the compositor is to then draw the composited off-screen target back to the
 * default target.
 *
 * @todo Allow optionally requesting more target attachments beyond the default
 * color buffer.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC CompositorWidget : public GuiWidget
{
public:
    CompositorWidget(const String &name = String());

    GLTexture &composite() const;

    /**
     * Sets the matrix that is used when drawing the composited contents
     * back to the normal render target.
     *
     * @param projMatrix  Projection matrix.
     */
    void setCompositeProjection(const Mat4f &projMatrix);

    /**
     * Sets the projection used for displaying the composited content to the
     * default matrix (covering the full view).
     */
    void useDefaultCompositeProjection();

    // Events.
    void viewResized();
    void preDrawChildren();
    void postDrawChildren();

protected:
    void glInit();
    void glDeinit();
    void drawComposite();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_COMPOSITORWIDGET_H
