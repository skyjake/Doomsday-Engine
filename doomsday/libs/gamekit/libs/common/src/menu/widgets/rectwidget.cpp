/** @file rectwidget.cpp  Simple rectangular widget with a background image.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "menu/widgets/rectwidget.h"

#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

DE_PIMPL_NOREF(RectWidget)
{
    Vec2ui dimensions;  ///< Dimensions of the rectangle.
    patchid_t patch = 0;   ///< Background patch.
};

RectWidget::RectWidget(patchid_t backgroundPatch)
    : Widget()
    , d(new Impl)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);

    setBackgroundPatch(backgroundPatch);
}

RectWidget::~RectWidget()
{}

void RectWidget::draw() const
{
    const Vec2i origin = geometry().topLeft;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(origin.x, origin.y, 0);

    if(d->patch)
    {
        DGL_SetPatch(d->patch, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_Enable(DGL_TEXTURE_2D);
    }

    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    DGL_DrawRect2(0, 0, d->dimensions.x, d->dimensions.y);

    if(d->patch)
    {
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(-origin.x, -origin.y, 0);
}

void RectWidget::updateGeometry()
{
    if(d->dimensions == Vec2ui(0, 0))
    {
        // Inherit dimensions from the patch.
        patchinfo_t info;
        if(R_GetPatchInfo(d->patch, &info))
        {
            d->dimensions = Vec2ui(info.geometry.size.width, info.geometry.size.height);
        }
    }
    geometry().setSize(d->dimensions);
}

void RectWidget::setBackgroundPatch(patchid_t newBackgroundPatch)
{
    d->patch = newBackgroundPatch;
}

} // namespace menu
} // namespace common
