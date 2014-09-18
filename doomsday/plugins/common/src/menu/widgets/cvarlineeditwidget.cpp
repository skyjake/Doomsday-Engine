/** @file cvarlineeditwidget.cpp  UI widget for an editable line of text in a cvar.
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

#include "common.h"
#include "menu/widgets/cvarlineeditwidget.h"

#include "hu_menu.h" // Hu_MenuDefaultFocusAction

using namespace de;

namespace common {
namespace menu {

CVarLineEditWidget::CVarLineEditWidget(char const *cvarPath)
    : LineEditWidget()
    , _cvarPath(cvarPath)
{
    setAction(MNA_MODIFIED, CVarLineEditWidget_UpdateCVar);
    setAction(MNA_FOCUS,    Hu_MenuDefaultFocusAction);
}

CVarLineEditWidget::~CVarLineEditWidget()
{}

char const *CVarLineEditWidget::cvarPath() const
{
    return _cvarPath;
}

void CVarLineEditWidget_UpdateCVar(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);
    CVarLineEditWidget const &edit = wi->as<CVarLineEditWidget>();
    cvartype_t varType = Con_GetVariableType(edit.cvarPath());

    if(Widget::MNA_MODIFIED != action) return;

    switch(varType)
    {
    case CVT_CHARPTR:
        Con_SetString2(edit.cvarPath(), edit.text().toUtf8().constData(), SVF_WRITE_OVERRIDE);
        break;

    case CVT_URIPTR: {
        /// @todo Sanitize and validate against known schemas.
        de::Uri uri(edit.text(), RC_NULL);
        Con_SetUri2(edit.cvarPath(), reinterpret_cast<uri_s *>(&uri), SVF_WRITE_OVERRIDE);
        break; }

    default: break;
    }
}

} // namespace menu
} // namespace common
