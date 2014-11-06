/** @file keygrabberwidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/dd_input.h"
#include "ui/b_util.h"

#include <de/KeyEvent>
#include <de/ddstring.h>

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(KeyGrabberWidget)
{
    Instance(Public *i) : Base(i)
    {}

    void focus()
    {
        root().setFocus(thisPublic);
        self.set(Background(Background::GradientFrame, style().colors().colorf("accent"), 6));
        self.setText(tr("Waiting for a key..."));
    }

    void unfocus()
    {
        root().setFocus(0);
        self.set(Background());
        self.setText(tr("Click to focus"));
    }
};

KeyGrabberWidget::KeyGrabberWidget(String const &name)
    : LabelWidget(name), d(new Instance(this))
{
    setTextLineAlignment(AlignLeft);
    setText(tr("Click to focus"));
}

bool KeyGrabberWidget::handleEvent(Event const &event)
{
    if(!hasFocus())
    {
        switch(handleMouseClick(event))
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
        if(KeyEvent const *key = event.maybeAs<KeyEvent>())
        {
            if(key->ddKey() == DDKEY_ESCAPE)
            {
                d->unfocus();
                return true;
            }

            ddevent_t ev;
            InputSystem::convertEvent(event, &ev);

            String info = String("DD:%1 Qt:0x%2 Native:0x%3\n" _E(m) "%4")
                              .arg(key->ddKey())
                              .arg(key->qtKey(), 0, 16)
                              .arg(key->nativeCode(), 0, 16)
                              .arg(B_EventToString(ev));
            setText(info);

            return true;
        }

        if(MouseEvent const *mouse = event.maybeAs<MouseEvent>())
        {
            if(mouse->type() == Event::MouseButton &&
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
