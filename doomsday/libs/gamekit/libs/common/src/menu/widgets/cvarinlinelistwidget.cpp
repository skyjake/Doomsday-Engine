/** @file cvarinlinelistwidget.cpp  UI widget for a selectable, inline list of items.
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
#include "menu/widgets/cvarinlinelistwidget.h"

#include "hu_menu.h" // Hu_MenuDefaultFocusAction

using namespace de;

namespace common {
namespace menu {

CVarInlineListWidget::CVarInlineListWidget(const char *cvarPath, int cvarValueMask)
    : InlineListWidget()
    , _cvarPath(cvarPath)
    , _cvarValueMask(cvarValueMask)
{
    setColor(MENU_COLOR3);
    setAction(Modified,    CVarInlineListWidget_UpdateCVar);
    setAction(FocusGained, Hu_MenuDefaultFocusAction);
}

CVarInlineListWidget::~CVarInlineListWidget()
{}

const char *CVarInlineListWidget::cvarPath() const
{
    return _cvarPath;
}

int CVarInlineListWidget::cvarValueMask() const
{
    return _cvarValueMask;
}

void CVarInlineListWidget_UpdateCVar(Widget &wi, Widget::Action action)
{
    const CVarInlineListWidget *list = &wi.as<CVarInlineListWidget>();

    if(action != Widget::Modified) return;

    if(list->selection() < 0) return; // Hmm?

    cvartype_t varType = Con_GetVariableType(list->cvarPath());
    if(CVT_NULL == varType) return;

    const ListWidget::Item *item = list->items()[list->selection()];
    int value;
    if(list->cvarValueMask())
    {
        value = Con_GetInteger(list->cvarPath());
        value = (value & ~list->cvarValueMask()) | (item->userValue() & list->cvarValueMask());
    }
    else
    {
        value = item->userValue();
    }

    switch(varType)
    {
    case CVT_INT:
        Con_SetInteger2(list->cvarPath(), value, SVF_WRITE_OVERRIDE);
        break;
    case CVT_BYTE:
        Con_SetInteger2(list->cvarPath(), (byte) value, SVF_WRITE_OVERRIDE);
        break;

    default: Con_Error("CVarInlineListWidget_UpdateCVar: Unsupported variable type %i", (int)varType);
    }
}

} // namespace menu
} // namespace common
