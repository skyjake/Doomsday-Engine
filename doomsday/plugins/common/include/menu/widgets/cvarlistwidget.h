/** @file cvarlistwidget.h  UI widget for a selectable, list of items.
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

#ifndef LIBCOMMON_UI_CVARLISTWIDGET
#define LIBCOMMON_UI_CVARLISTWIDGET

#include "listwidget.h"

namespace common {
namespace menu {

struct CVarListWidget : public ListWidget
{
public:
    CVarListWidget(char const *cvarPath, int cvarValueMask = 0);
    virtual ~CVarListWidget();

    char const *cvarPath() const;
    int cvarValueMask() const;

private:
    char const *_cvarPath;
    int _cvarValueMask;
};

void CvarListWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action);

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_CVARLISTWIDGET
