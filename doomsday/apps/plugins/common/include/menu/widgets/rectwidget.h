/** @file rectwidget.h  Simple rectangular widget with a background image.
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

#ifndef LIBCOMMON_UI_RECTWIDGET
#define LIBCOMMON_UI_RECTWIDGET

#include "widget.h"

namespace common {
namespace menu {

/**
 * A simple rectangluar widget with a background.
 *
 * @ingroup menu
 */
class RectWidget : public Widget
{
public:
    explicit RectWidget(patchid_t backgroundPatch = 0);
    virtual ~RectWidget();

    void draw() const;
    void updateGeometry();

    /**
     * Apply the Patch graphic referenced by @a patch as the background for this rect.
     *
     * @param newBackgroundPatch  Unique identifier of the patch. If @c <= 0 the current
     * background will be cleared and the Rect will be drawn as a solid color.
     */
    void setBackgroundPatch(patchid_t newBackgroundPatch);

private:
    DE_PRIVATE(d)
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_WIDGET
