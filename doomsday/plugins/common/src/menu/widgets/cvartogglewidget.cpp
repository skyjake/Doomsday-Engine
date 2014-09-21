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

DENG2_PIMPL_NOREF(CVarToggleWidget)
{
    State state          = Up;
    char const *cvarPath = nullptr;
    int cvarValueMask    = 0;
    String downText;
    String upText;
};

CVarToggleWidget::CVarToggleWidget(char const *cvarPath, int cvarValueMask,
                                   String const &downText, String const &upText)
    : ButtonWidget()
    , d(new Instance)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR3);
    setAction(MNA_MODIFIED, CVarToggleWidget_UpdateCVar);
    setAction(MNA_FOCUS,    Hu_MenuDefaultFocusAction);

    d->cvarPath      = cvarPath;
    d->cvarValueMask = cvarValueMask;
    setDownText(downText);
    setUpText(upText);
}

CVarToggleWidget::~CVarToggleWidget()
{}

int CVarToggleWidget::handleCommand(menucommand_e cmd)
{
    if(cmd == MCMD_SELECT)
    {
        bool justActivated = false;
        if(!isActive())
        {
            justActivated = true;
            S_LocalSound(SFX_MENU_CYCLE, NULL);

            setFlags(Active);
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }

        if(!justActivated)
        {
            setFlags(Active, isActive()? UnsetFlags : SetFlags);
        }

        setState(isActive()? Down : Up);
        if(hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED);
        }

        if(!justActivated && !isActive())
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            if(hasAction(MNA_ACTIVEOUT))
            {
                execAction(MNA_ACTIVEOUT);
            }
        }

        return true;
    }

    return false; // Not eaten.
}

void CVarToggleWidget_UpdateCVar(Widget *wi, Widget::mn_actionid_t action)
{
    CVarToggleWidget *tog = &wi->as<CVarToggleWidget>();

    if(Widget::MNA_MODIFIED != action) return;

    tog->setText(tog->isDown()? tog->downText() : tog->upText());

    cvartype_t varType = Con_GetVariableType(tog->cvarPath());
    if(CVT_NULL == varType) return;

    int value;
    if(int const valueMask = tog->cvarValueMask())
    {
        value = Con_GetInteger(tog->cvarPath());
        if(tog->isDown())
        {
            value |= valueMask;
        }
        else
        {
            value &= ~valueMask;
        }
    }
    else
    {
        value = int(tog->state());
    }

    Con_SetInteger2(tog->cvarPath(), value, SVF_WRITE_OVERRIDE);
}

void CVarToggleWidget::setState(State newState)
{
    d->state = newState;
}

CVarToggleWidget::State CVarToggleWidget::state() const
{
    return d->state;
}

char const *CVarToggleWidget::cvarPath() const
{
    return d->cvarPath;
}

int CVarToggleWidget::cvarValueMask() const
{
    return d->cvarValueMask;
}

void CVarToggleWidget::setDownText(String const &newDownText)
{
    d->downText = newDownText;
}

String CVarToggleWidget::downText() const
{
    return d->downText;
}

void CVarToggleWidget::setUpText(String const &newUpText)
{
    d->upText = newUpText;
}

String CVarToggleWidget::upText() const
{
    return d->upText;
}

} // namespace menu
} // namespace common
