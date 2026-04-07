/** @file cvartogglewidget.cpp  Button widget for toggling cvars.
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
#include "menu/widgets/cvartogglewidget.h"

#include "hu_menu.h" // Hu_MenuDefaultFocusAction

using namespace de;

namespace common {
namespace menu {

DE_PIMPL_NOREF(CVarToggleWidget)
{
    State       state         = Up;
    const char *cvarPath      = nullptr;
    int         cvarValueMask = 0;
    String      downText;
    String      upText;

    std::function<void(State)> stateChangeCallback;
};

CVarToggleWidget::CVarToggleWidget(const char *cvarPath, int cvarValueMask,
                                   const String &downText, const String &upText)
    : ButtonWidget()
    , d(new Impl)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR3);
    setAction(Modified,    CVarToggleWidget_UpdateCVar);
    setAction(FocusGained, Hu_MenuDefaultFocusAction);

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
            execAction(Activated);
        }

        if(!justActivated)
        {
            setFlags(Active, isActive()? UnsetFlags : SetFlags);
        }

        setState(isActive()? Down : Up);
        execAction(Modified);

        if(!justActivated && !isActive())
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            execAction(Deactivated);
        }

        return true;
    }

    return false; // Not eaten.
}

void CVarToggleWidget_UpdateCVar(Widget &wi, Widget::Action action)
{
    CVarToggleWidget *tog = &wi.as<CVarToggleWidget>();

    if(action != Widget::Modified) return;

    tog->setText(tog->isDown()? tog->downText() : tog->upText());

    if(Con_GetVariableType(tog->cvarPath()) == CVT_NULL)
    {
        return;
    }

    int value;
    if(const int valueMask = tog->cvarValueMask())
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
    if (newState != d->state)
    {
        d->state = newState;

        if (d->stateChangeCallback)
        {
            d->stateChangeCallback(newState);
        }
    }
}

CVarToggleWidget::State CVarToggleWidget::state() const
{
    return d->state;
}

const char *CVarToggleWidget::cvarPath() const
{
    return d->cvarPath;
}

int CVarToggleWidget::cvarValueMask() const
{
    return d->cvarValueMask;
}

void CVarToggleWidget::setDownText(const String &newDownText)
{
    d->downText = newDownText;
}

String CVarToggleWidget::downText() const
{
    return d->downText;
}

void CVarToggleWidget::setUpText(const String &newUpText)
{
    d->upText = newUpText;
}

String CVarToggleWidget::upText() const
{
    return d->upText;
}

void CVarToggleWidget::setStateChangeCallback(const std::function<void (State)> &stateChanged)
{
    d->stateChangeCallback = stateChanged;
}

void CVarToggleWidget::pageActivated()
{
    ButtonWidget::pageActivated();
    setFlags(Active, isDown()? SetFlags : UnsetFlags);
}

} // namespace menu
} // namespace common
