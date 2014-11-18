/** @file cvarcoloreditwidget.h  UI widget for editing a color cvar.
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

#ifndef LIBCOMMON_UI_CVARCOLOREDITWIDGET
#define LIBCOMMON_UI_CVARCOLOREDITWIDGET

#include "coloreditwidget.h"

namespace common {
namespace menu {

/**
 * UI widget for editing a color cvar.
 *
 * @ingroup menu
 */
class CVarColorEditWidget : public ColorEditWidget
{
public:
    explicit CVarColorEditWidget(char const *redCVarPath,  char const *greenCVarPath,
                                 char const *blueCVarPath, char const *alphaCVarPath = 0,
                                 de::Vector4f const &color = de::Vector4f(),
                                 bool rgbaMode = false);
    virtual ~CVarColorEditWidget();

    char const *cvarPath(int component) const;

    inline char const *redCVarPath()   const { return cvarPath(0); }
    inline char const *greenCVarPath() const { return cvarPath(1); }
    inline char const *blueCVarPath()  const { return cvarPath(2); }
    inline char const *alphaCVarPath() const { return cvarPath(3); }

private:
    char const *_cvarPaths[4];
};

void CVarColorEditWidget_UpdateCVar(Widget &wi, Widget::Action action);

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_CVARCOLOREDITWIDGET
