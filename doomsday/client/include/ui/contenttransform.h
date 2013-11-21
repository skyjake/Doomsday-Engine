/** @file contenttransform.h  Base class for window content transformation.
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

#ifndef DENG_CLIENT_UI_CONTENTTRANSFORM_H
#define DENG_CLIENT_UI_CONTENTTRANSFORM_H

#include <de/Vector>

class ClientWindow;

/**
 * Base class for window content transformation.
 */
class ContentTransform
{
public:
    ContentTransform(ClientWindow &window);

    ClientWindow &window() const;

    /**
     * Called by the window when GL is ready.
     */
    virtual void glInit();

    virtual void glDeinit();

    /**
     * Determines how large the root widget should be for a particular canvas size.
     *
     * @param physicalCanvasSize  Canvas size (pixels).
     *
     * @return Logical size (UI units).
     */
    virtual de::Vector2ui logicalRootSize(de::Vector2ui const &physicalCanvasSize) const;

    /**
     * Translate a point in physical window coordinates to logical coordinates.
     *
     * @param pos  Window coordinates (pixels).
     *
     * @return Logical coordinates inside the root widget's area.
     */
    virtual de::Vector2f windowToLogicalCoords(de::Vector2i const &pos) const;

    virtual void drawTransformed();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_CONTENTTRANSFORM_H
