/** @file vrwindowtransform.h  Window content transformation for virtual reality.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
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

#ifndef LIBAPPFW_VRWINDOWTRANSFORM_H
#define LIBAPPFW_VRWINDOWTRANSFORM_H

#include "libgui.h"
#include "de/windowtransform.h"

namespace de {

class GLTextureFramebuffer;

/**
 * Window content transformation for virtual reality.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC VRWindowTransform : public WindowTransform
{
public:
    VRWindowTransform(BaseWindow &window);

    void glInit() override;
    void glDeinit() override;

    Vec2ui logicalRootSize(const Vec2ui &physicalWindowSize) const override;
    Vec2f  windowToLogicalCoords(const Vec2i &pos) const override;
    Vec2f  logicalToWindowCoords(const Vec2i &pos) const override;

    void drawTransformed() override;

    GLTextureFramebuffer &unwarpedFramebuffer();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VRWINDOWTRANSFORM_H
