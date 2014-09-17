/** @file buttonwidget.h  Button widget.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_UI_BUTTONWIDGET
#define LIBCOMMON_UI_BUTTONWIDGET

#include <QVariant>
#include "widget.h"

namespace common {
namespace menu {

/**
 * @ingroup menu
 */
class ButtonWidget : public Widget
{
public:
    explicit ButtonWidget(de::String const &text = "", patchid_t patch = 0);
    virtual ~ButtonWidget();

    void draw(Point2Raw const *origin);
    void updateGeometry(Page *pagePtr);
    int handleCommand(menucommand_e command);

    ButtonWidget &setText(de::String const &newText);
    de::String text() const;

    ButtonWidget &setPatch(patchid_t newPatch);
    patchid_t patch() const;

    ButtonWidget &setNoAltText(bool yes = true);
    bool noAltText() const;

    void setData(QVariant const &d);
    QVariant const &data() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_BUTTONWIDGET
