/** @file cvartogglewidget.cpp  Button widget for toggling cvars.
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
#include "menu/widgets/cvartogglewidget.h"

#include "hu_menu.h" // Hu_MenuDefaultFocusAction

using namespace de;

namespace common {
namespace menu {

CVarToggleWidget::CVarToggleWidget(char const *cvarPath)
    : ButtonWidget()
    , _cvarPath(cvarPath)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR3;
    Widget::actions[MNA_MODIFIED].callback = CVarToggleWidget_UpdateCVar;
    Widget::actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
}

CVarToggleWidget::~CVarToggleWidget()
{}

char const *CVarToggleWidget::cvarPath() const
{
    return _cvarPath;
}

int CVarToggleWidget::handleCommand(menucommand_e cmd)
{
    if(cmd == MCMD_SELECT)
    {
        bool justActivated = false;
        if(!(Widget::_flags & MNF_ACTIVE))
        {
            justActivated = true;
            S_LocalSound(SFX_MENU_CYCLE, NULL);

            Widget::_flags |= MNF_ACTIVE;
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }

        if(!justActivated)
        {
            Widget::_flags ^= MNF_ACTIVE;
        }

        if(Widget::data1)
        {
            void *data = Widget::data1;

            *((char *)data) = (Widget::_flags & MNF_ACTIVE) != 0;
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
        }

        if(!justActivated && !(Widget::_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            if(hasAction(MNA_ACTIVEOUT))
            {
                execAction(MNA_ACTIVEOUT);
            }
        }

        Widget::timer = 0;
        return true;
    }

    return false; // Not eaten.
}

void CVarToggleWidget_UpdateCVar(Widget *wi, Widget::mn_actionid_t action)
{
    CVarToggleWidget *tog = &wi->as<CVarToggleWidget>();
    cvarbutton_t const *cb = (cvarbutton_t *)wi->data1;
    cvartype_t varType = Con_GetVariableType(tog->cvarPath());
    int value;

    if(Widget::MNA_MODIFIED != action) return;

    tog->setText(cb->active? cb->yes : cb->no);

    if(CVT_NULL == varType) return;

    if(cb->mask)
    {
        value = Con_GetInteger(tog->cvarPath());
        if(cb->active)
        {
            value |= cb->mask;
        }
        else
        {
            value &= ~cb->mask;
        }
    }
    else
    {
        value = cb->active;
    }

    Con_SetInteger2(tog->cvarPath(), value, SVF_WRITE_OVERRIDE);
}

} // namespace menu
} // namespace common
