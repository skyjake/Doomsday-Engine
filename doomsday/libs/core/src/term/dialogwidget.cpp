/** @file term/dialogwidget.cpp  Base class for modal dialogs.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/term/dialogwidget.h"
#include "de/term/textrootwidget.h"
#include "de/term/keyevent.h"

#include "de/eventloop.h"

namespace de { namespace term {

DE_PIMPL_NOREF(DialogWidget)
{
    EventLoop subloop;

    DE_PIMPL_AUDIENCES(Accept, Reject)
};
DE_AUDIENCE_METHODS(DialogWidget, Accept, Reject)

DialogWidget::DialogWidget(const String &name)
    : Widget(name), d(new Impl)
{
    // Dialogs are hidden until executed.
    hide();
}

void DialogWidget::prepare()
{
    show();
    root().setFocus(this);
    redraw();
}

void DialogWidget::finish(int /*result*/)
{
    hide();
    root().setFocus(nullptr);
}

int DialogWidget::exec(TextRootWidget &root)
{
    // The widget is added to the root temporarily (as top child).
    DE_ASSERT(!hasRoot());
    root.add(this);

    // Center the dialog.
    rule().setInput(Rule::Left, (root.viewWidth()  - rule().width())  / 2)
          .setInput(Rule::Top,  (root.viewHeight() - rule().height()) / 2);

    prepare();

    int result = d->subloop.exec();

    finish(result);

    // No longer in the root.
    root.remove(*this);
    root.requestDraw();
    return result;
}

void DialogWidget::draw()
{
    Rectanglei pos = rule().recti().adjusted(Vec2i(-2, -1), Vec2i(2, 1));

    // Draw a background frame.
    targetCanvas().fill(pos, TextCanvas::AttribChar());
    targetCanvas().drawLineRect(pos);
}

bool DialogWidget::handleEvent(const Event &event)
{
    if (event.type() == Event::KeyPress)
    {
        const KeyEvent &ev = event.as<KeyEvent>();
        if (ev.key() == Key::Escape)
        {
            reject();
            return true;
        }
    }
    // All events not handled by children are eaten by the dialog.
    return true;
}

void DialogWidget::accept(int result)
{
    if (d->subloop.isRunning())
    {
        d->subloop.quit(result);
        DE_NOTIFY(Accept, i) { i->accepted(result); }
    }
}

void DialogWidget::reject(int result)
{
    if (d->subloop.isRunning())
    {
        d->subloop.quit(result);
        DE_NOTIFY(Reject, i) { i->rejected(result); }
    }
}

}} // namespace de::term
