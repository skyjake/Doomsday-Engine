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

#include <QMap>
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
    Vector2i origin;           ///< Used with the fixed layout (in the page coordinate space).

    Rectanglei geometry;       ///< "Physical" geometry.
    Page *page = nullptr;      ///< Page which owns this object (if any).

    String helpInfo;           ///< Additional help information displayed when the widget has focus.
    int shortcut     = 0;      ///< DDKEY used to switch focus (@c 0= none).
    int pageFontIdx  = 0;      ///< Index of the predefined page font to use when drawing this.
    int pageColorIdx = 0;      ///< Index of the predefined page color to use when drawing this.

    typedef QMap<Action, ActionCallback> Actions;
    Actions actions;

    OnTickCallback onTickCallback = nullptr;
    CommandResponder cmdResponder = nullptr;

    // User data values.
    QVariant userValue;
    QVariant userValue2;
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
        if(int result = d->cmdResponder(*this, command))
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
    if(isPaused()) return;

    // Process the tick.
    if(d->onTickCallback)
    {
        d->onTickCallback(*this);
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
    /// @throw MissingPageError No Page is presently attributed.
    throw MissingPageError("Widget::page", "No page is attributed");
}

Rectanglei &Widget::geometry()
{
    return d->geometry;
}

Rectanglei const &Widget::geometry() const
{
    return d->geometry;
}

Widget &Widget::setFixedOrigin(Vector2i const &newOrigin)
{
    d->origin = newOrigin;
    return *this;
}

Vector2i Widget::fixedOrigin() const
{
    return d->origin;
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

Widget::Flags Widget::flags() const
{
    return d->flags;
}

Widget &Widget::setGroup(int newGroup)
{
    d->group = newGroup;
    return *this;
}

int Widget::group() const
{
    return d->group;
}

Widget &Widget::setShortcut(int ddkey)
{
    if(isalnum(ddkey))
    {
        d->shortcut = tolower(ddkey);
    }
    return *this;
}

int Widget::shortcut()
{
    return d->shortcut;
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

Widget &Widget::setHelpInfo(String newHelpInfo)
{
    d->helpInfo = newHelpInfo;
    return *this;
}

String const &Widget::helpInfo() const
{
    return d->helpInfo;
}

int Widget::handleCommand(menucommand_e cmd)
{
    if(MCMD_SELECT == cmd && isFocused() && !isDisabled())
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        if(!isActive())
        {
            setFlags(Active);
            execAction(Activated);
        }

        setFlags(Active, UnsetFlags);
        execAction(Deactivated);
        return true;
    }
    return false; // Not eaten.
}

bool Widget::hasAction(Action id) const
{
    DENG2_ASSERT(id >= Modified && id <= FocusGained);
    return d->actions.contains(id);
}

Widget &Widget::setAction(Action id, ActionCallback callback)
{
    DENG2_ASSERT(id >= Modified && id <= FocusGained);
    if(callback)
    {
        d->actions.insert(id, callback);
    }
    else
    {
        d->actions.remove(id);
    }
    return *this;
}

void Widget::execAction(Action id)
{
    if(hasAction(id))
    {
        d->actions[id](*this, id);
    }
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
