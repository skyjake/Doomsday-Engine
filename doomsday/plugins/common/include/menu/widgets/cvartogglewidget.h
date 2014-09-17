/** @file cvartogglewidget.h  Button widget for toggling cvars.
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

#ifndef LIBCOMMON_UI_CVARTOGGLEWIDGET
#define LIBCOMMON_UI_CVARTOGGLEWIDGET

#include "buttonwidget.h"

namespace common {
namespace menu {

struct CVarToggleWidget : public ButtonWidget
{
public:
    CVarToggleWidget(char const *cvarPath);
    virtual ~CVarToggleWidget();

    int handleCommand(menucommand_e command);

    char const *cvarPath() const;

private:
    char const *_cvarPath;
};

void CVarToggleWidget_UpdateCVar(Widget *wi, Widget::mn_actionid_t action);

/// @todo Refactor away.
struct cvarbutton_t
{
    char active;
    char const *cvarname;
    char const *yes;
    char const *no;
    int mask;

    cvarbutton_t(char active = 0, char const *cvarname = 0, char const *yes = 0, char const *no = 0,
                 int mask = 0)
        : active(active)
        , cvarname(cvarname)
        , yes(yes)
        , no(no)
        , mask(mask)
    {}
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_CVARTOGGLEWIDGET
