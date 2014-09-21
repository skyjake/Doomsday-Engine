/** @file widget.cpp  Base class for widgets.
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
#include "menu/widgets/widget.h"

#include "hu_menu.h" // menu sounds

using namespace de;

namespace common {
namespace menu {

DENG2_PIMPL_NOREF(Widget)
{
    Flags flags;
    int group = 0;             ///< Object group identifier.
    Point2Raw origin;          ///< Used with the fixed layout (in the page coordinate space).

    Rect *geometry = nullptr;  ///< Current geometry.
    Page *page     = nullptr;  ///< Page which owns this object (if any).

    String helpInfo;           ///< Additional help information displayed when the widget has focus.
    int shortcut     = 0;      ///< DDKEY used to switch focus (@c 0= none).
    int pageFontIdx  = 0;      ///< Index of the predefined page font to use when drawing this.
    int pageColorIdx = 0;      ///< Index of the predefined page color to use when drawing this.

    mn_actioninfo_t actions[MNACTION_COUNT];

    OnTickCallback onTickCallback = nullptr;
    CommandResponder cmdResponder = nullptr;

    // User data values.
    QVariant userValue;
    QVariant userValue2;

    Instance()
    {
        geometry = Rect_New();
        de::zap(actions);
    }

    ~Instance() { Rect_Delete(geometry); }
};

Widget::Widget() : d(new Instance)
{}

Widget::~Widget()
{}

int Widget::handleEvent(event_t * /*ev*/)
{
    return 0; // Not handled.
}

int Widget::handleEvent_Privileged(event_t * /*ev*/)
{
    return 0; // Not handled.
}

Widget &Widget::setCommandResponder(CommandResponder newResponder)
{
    d->cmdResponder = newResponder;
    return *this;
}

int Widget::cmdResponder(menucommand_e command)
{
    if(d->cmdResponder)
    {
        if(int result = d->cmdResponder(this, command))
            return result;
    }
    else
    {
        if(int result = handleCommand(command))
            return result;
    }
    return false; // Not handled.
}

void Widget::tick()
{
    // Hidden widgets do not tick.
    if(isHidden()) return;

    // Paused widgets do not tick.
    if(flags() & Paused) return;

    // Process the tick.
    if(d->onTickCallback)
    {
        d->onTickCallback(this);
    }
}

Widget &Widget::setOnTickCallback(OnTickCallback newCallback)
{
    d->onTickCallback = newCallback;
    return *this;
}

bool Widget::hasPage() const
{
    return d->page != 0;
}

Widget &Widget::setPage(Page *newPage)
{
    d->page = newPage;
    return *this;
}

Page &Widget::page() const
{
    if(d->page)
    {
        return *d->page;
    }
    throw Error("Widget::page", "No page is attributed");
}

Widget::Flags Widget::flags() const
{
    return d->flags;
}

Rect *Widget::geometry()
{
    return d->geometry;
}

Rect const *Widget::geometry() const
{
    return d->geometry;
}

Point2Raw const *Widget::fixedOrigin() const
{
    return &d->origin;
}

Widget &Widget::setFixedOrigin(Point2Raw const *newOrigin)
{
    if(newOrigin)
    {
        d->origin.x = newOrigin->x;
        d->origin.y = newOrigin->y;
    }
    return *this;
}

Widget &Widget::setFixedX(int newX)
{
    d->origin.x = newX;
    return *this;
}

Widget &Widget::setFixedY(int newY)
{
    d->origin.y = newY;
    return *this;
}

Widget &Widget::setFlags(Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
    return *this;
}

int Widget::group() const
{
    return d->group;
}

Widget &Widget::setGroup(int newGroup)
{
    d->group = newGroup;
    return *this;
}

int Widget::shortcut()
{
    return d->shortcut;
}

Widget &Widget::setShortcut(int ddkey)
{
    if(isalnum(ddkey))
    {
        d->shortcut = tolower(ddkey);
    }
    return *this;
}

Widget &Widget::setFont(int newPageFont)
{
    d->pageFontIdx = newPageFont;
    return *this;
}

int Widget::font() const
{
    return d->pageFontIdx;
}

Widget &Widget::setColor(int newPageColor)
{
    d->pageColorIdx = newPageColor;
    return *this;
}

int Widget::color() const
{
    return d->pageColorIdx;
}

String const &Widget::helpInfo() const
{
    return d->helpInfo;
}

Widget &Widget::setHelpInfo(String newHelpInfo)
{
    d->helpInfo = newHelpInfo;
    return *this;
}

int Widget::handleCommand(menucommand_e cmd)
{
    if(MCMD_SELECT == cmd && isFocused() && !isDisabled())
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        if(!isActive())
        {
            setFlags(Active);
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }

        setFlags(Active, UnsetFlags);
        if(hasAction(MNA_ACTIVEOUT))
        {
            execAction(MNA_ACTIVEOUT);
        }
        return true;
    }
    return false; // Not eaten.
}

Widget::mn_actioninfo_t const *Widget::action(mn_actionid_t id)
{
    DENG2_ASSERT((id) >= MNACTION_FIRST && (id) <= MNACTION_LAST);
    return &d->actions[id];
}

bool Widget::hasAction(mn_actionid_t id)
{
    mn_actioninfo_t const *info = action(id);
    return (info && info->callback != 0);
}

Widget &Widget::setAction(mn_actionid_t id, mn_actioninfo_t::ActionCallback callback)
{
    DENG2_ASSERT((id) >= MNACTION_FIRST && (id) <= MNACTION_LAST);
    d->actions[id].callback = callback;
    return *this;
}

void Widget::execAction(mn_actionid_t id)
{
    if(hasAction(id))
    {
        action(id)->callback(this, id);
        return;
    }
    DENG2_ASSERT(!"MNObject::ExecAction: Attempt to execute non-existent action.");
}

Widget &Widget::setUserValue(QVariant const &newValue)
{
    d->userValue = newValue;
    return *this;
}

QVariant const &Widget::userValue() const
{
    return d->userValue;
}

Widget &Widget::setUserValue2(QVariant const &newValue)
{
    d->userValue2 = newValue;
    return *this;
}

QVariant const &Widget::userValue2() const
{
    return d->userValue2;
}

} // namespace menu
} // namespace common
