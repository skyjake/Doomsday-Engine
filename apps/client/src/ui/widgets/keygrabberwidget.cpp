/** @file keygrabberwidget.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/widgets/keygrabberwidget.h"
#include "clientapp.h"
#include "ui/ddevent.h"
#include "ui/b_util.h"
#include "ui/inputsystem.h"

#include <de/keyevent.h>
#include <de/legacy/ddstring.h>

using namespace de;
using namespace de::ui;

DE_GUI_PIMPL(KeyGrabberWidget)
{
    Impl(Public *i) : Base(i)
    {}

    void focus()
    {
        root().setFocus(thisPublic);
        self().set(Background(Background::GradientFrame, style().colors().colorf("accent"), 6));
        self().setText("Waiting for a key...");
    }

    void unfocus()
    {
        root().setFocus(0);
        self().set(Background());
        self().setText("Click to focus");
    }
};

KeyGrabberWidget::KeyGrabberWidget(const String &name)
    : LabelWidget(name), d(new Impl(this))
{
    setBehavior(Focusable);
    setTextLineAlignment(AlignLeft);
    setText("Click to focus");
}

bool KeyGrabberWidget::handleEvent(const Event &event)
{
    if (!hasFocus())
    {
        switch (handleMouseClick(event))
        {
        case MouseClickFinished:
            d->focus();
            return true;

        default:
            return false;
        }
    }
    else
    {
        if (const KeyEvent *key = maybeAs<KeyEvent>(event))
        {
            if (key->ddKey() == DDKEY_ESCAPE)
            {
                d->unfocus();
                return true;
            }

            ddevent_t ev;
            InputSystem::convertEvent(event, ev);

            String info = Stringf("DD:%i SDL:0x%x Native:0x%x\n" _E(m) "%s",
                              key->ddKey(),
                              key->sdlKey(),
                              key->scancode(),
                              B_EventToString(ev).c_str());
            setText(info);

            return true;
        }

        if (const MouseEvent *mouse = maybeAs<MouseEvent>(event))
        {
            if (mouse->type() == Event::MouseButton &&
               mouse->state() == MouseEvent::Released &&
               !hitTest(event))
            {
                // Clicking outside clears focus.
                d->unfocus();
                return true;
            }
        }
    }
    return false;
}
