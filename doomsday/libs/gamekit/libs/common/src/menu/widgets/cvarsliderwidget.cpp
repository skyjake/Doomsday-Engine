/** @file cvarsliderwidget.cpp  UI widget for manipulating a cvar with a graphical slider.
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
#include "menu/widgets/cvarsliderwidget.h"

#include "hu_menu.h" // Hu_MenuDefaultFocusAction

using namespace de;

namespace common {
namespace menu {

CVarSliderWidget::CVarSliderWidget(const char *cvarPath, float min, float max, float step, bool floatMode)
    : SliderWidget(min, max, step, floatMode)
    , _cvarPath(cvarPath)
{
    setAction(Modified,    CVarSliderWidget_UpdateCVar);
    setAction(FocusGained, Hu_MenuDefaultFocusAction);
}

CVarSliderWidget::~CVarSliderWidget()
{}

const char *CVarSliderWidget::cvarPath() const
{
    return _cvarPath;
}

void CVarSliderWidget_UpdateCVar(Widget &wi, Widget::Action action)
{
    if(action != Widget::Modified) return;

    CVarSliderWidget &sldr = wi.as<CVarSliderWidget>();
    cvartype_t varType = Con_GetVariableType(sldr.cvarPath());
    if(CVT_NULL == varType) return;

    float value = sldr.value();
    switch(varType)
    {
    case CVT_FLOAT:
        if(sldr.step() >= .01f)
        {
            Con_SetFloat2(sldr.cvarPath(), (int) (100 * value) / 100.0f, SVF_WRITE_OVERRIDE);
        }
        else
        {
            Con_SetFloat2(sldr.cvarPath(), value, SVF_WRITE_OVERRIDE);
        }
        break;

    case CVT_INT:
        Con_SetInteger2(sldr.cvarPath(), (int) value, SVF_WRITE_OVERRIDE);
        break;

    case CVT_BYTE:
        Con_SetInteger2(sldr.cvarPath(), (byte) value, SVF_WRITE_OVERRIDE);
        break;

    default: break;
    }
}

} // namespace menu
} // namespace common
