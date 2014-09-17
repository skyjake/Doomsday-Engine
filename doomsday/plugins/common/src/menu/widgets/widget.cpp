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
    int group = 0;    ///< Object group identifier.
    String helpInfo;  ///< Additional help information displayed when the widget has focus.
};

Widget::Widget()
    : _flags             (0)
    , _shortcut          (0)
    , _pageFontIdx       (0)
    , _pageColorIdx      (0)
    , onTickCallback     (0)
    , cmdResponder       (0)
    , data1              (0)
    , data2              (0)
    , _geometry          (0)
    , _page              (0)
    , timer              (0)
    , d(new Instance)
{
    de::zap(actions);
}

Widget::~Widget()
{
    Rect_Delete(_geometry);
}

int Widget::handleEvent(event_t * /*ev*/)
{
    return 0; // Not handled.
}

int Widget::handleEvent_Privileged(event_t * /*ev*/)
{
    return 0; // Not handled.
}

void Widget::tick()
{
    if(onTickCallback)
    {
        onTickCallback(this);
    }
}

bool Widget::hasPage() const
{
    return _page != 0;
}

Page &Widget::page() const
{
    if(_page)
    {
        return *_page;
    }
    throw Error("Widget::page", "No page is attributed");
}

int Widget::flags() const
{
    return _flags;
}

Rect const *Widget::geometry() const
{
    return _geometry;
}

Point2Raw const *Widget::fixedOrigin() const
{
    return &_origin;
}

Widget &Widget::setFixedOrigin(Point2Raw const *newOrigin)
{
    if(newOrigin)
    {
        _origin.x = newOrigin->x;
        _origin.y = newOrigin->y;
    }
    return *this;
}

Widget &Widget::setFixedX(int newX)
{
    _origin.x = newX;
    return *this;
}

Widget &Widget::setFixedY(int newY)
{
    _origin.y = newY;
    return *this;
}

Widget &Widget::setFlags(flagop_t op, int flagsToChange)
{
    switch(op)
    {
    case FO_CLEAR:  _flags &= ~flagsToChange;  break;
    case FO_SET:    _flags |= flagsToChange;   break;
    case FO_TOGGLE: _flags ^= flagsToChange;   break;

    default: DENG2_ASSERT(!"Widget::SetFlags: Unknown op.");
    }
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
    return _shortcut;
}

Widget &Widget::setShortcut(int ddkey)
{
    if(isalnum(ddkey))
    {
        _shortcut = tolower(ddkey);
    }
    return *this;
}

int Widget::font()
{
    return _pageFontIdx;
}

int Widget::color()
{
    return _pageColorIdx;
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
    if(MCMD_SELECT == cmd && (_flags & MNF_FOCUS) && !(_flags & MNF_DISABLED))
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        if(!(_flags & MNF_ACTIVE))
        {
            _flags |= MNF_ACTIVE;
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }

        _flags &= ~MNF_ACTIVE;
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
    return &actions[id];
}

dd_bool Widget::hasAction(mn_actionid_t id)
{
    mn_actioninfo_t const *info = action(id);
    return (info && info->callback != 0);
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

} // namespace menu
} // namespace common
