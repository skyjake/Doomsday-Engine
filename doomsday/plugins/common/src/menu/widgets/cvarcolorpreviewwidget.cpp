/** @file cvarcolorpreviewwidget.cpp  UI widget for previewing a color in a cvar.
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
#include "menu/widgets/cvarcolorpreviewwidget.h"

#include "hu_menu.h" // Hu_MenuDefaultFocusAction
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

CVarColorPreviewWidget::CVarColorPreviewWidget(char const *redCVarPath, char const *greenCVarPath,
    char const *blueCVarPath, char const *alphaCVarPath, Vector4f const &color, bool rgbaMode)
    : ColorPreviewWidget(color, rgbaMode)
{
    setAction(MNA_MODIFIED, CVarColorPreviewWidget_UpdateCVar);
    setAction(MNA_FOCUS,    Hu_MenuDefaultFocusAction);

    _cvarPaths[0] = redCVarPath;
    _cvarPaths[1] = greenCVarPath;
    _cvarPaths[2] = blueCVarPath;
    _cvarPaths[3] = alphaCVarPath;
}

CVarColorPreviewWidget::~CVarColorPreviewWidget()
{}

char const *CVarColorPreviewWidget::cvarPath(int component) const
{
    if(component >= 0 && component < 4)
    {
        return _cvarPaths[component];
    }
    return 0;
}

void CVarColorPreviewWidget_UpdateCVar(Widget *wi, Widget::mn_actionid_t action)
{
    CVarColorPreviewWidget *cbox = &wi->as<CVarColorPreviewWidget>();

    if(action != Widget::MNA_MODIFIED) return;

    Con_SetFloat2(cbox->redCVarPath(),   cbox->red(),   SVF_WRITE_OVERRIDE);
    Con_SetFloat2(cbox->greenCVarPath(), cbox->green(), SVF_WRITE_OVERRIDE);
    Con_SetFloat2(cbox->blueCVarPath(),  cbox->blue(),  SVF_WRITE_OVERRIDE);
    if(cbox->rgbaMode())
    {
        Con_SetFloat2(cbox->alphaCVarPath(), cbox->alpha(), SVF_WRITE_OVERRIDE);
    }
}

} // namespace menu
} // namespace common
