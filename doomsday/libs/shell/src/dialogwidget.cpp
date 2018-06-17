/** @file libshell/src/dialogwidget.cpp  Base class for modal dialogs.
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

#include "de/shell/DialogTextWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/KeyEvent"

namespace de { namespace shell {

DE_PIMPL_NOREF(DialogTextWidget)
{
//    QEventLoop subloop;

    DE_PIMPL_AUDIENCE(Accept)
    DE_PIMPL_AUDIENCE(Reject)
};

DE_AUDIENCE_METHOD(DialogTextWidget, Accept)
DE_AUDIENCE_METHOD(DialogTextWidget, Reject)

DialogTextWidget::DialogTextWidget(String const &name)
    : TextWidget(name), d(new Impl)
{
    // Dialogs are hidden until executed.
    hide();
}

void DialogTextWidget::prepare()
{
    show();
    root().setFocus(this);
    redraw();
}

void DialogTextWidget::finish(int /*result*/)
{
    hide();
    root().setFocus(0);
}

int DialogTextWidget::exec(TextRootWidget &root)
{
    // The widget is added to the root temporarily (as top child).
    DE_ASSERT(!hasRoot());
    root.add(this);

    // Center the dialog.
    rule().setInput(Rule::Left,  (root.viewWidth()  - rule().width())  / 2)
          .setInput(Rule::Top,   (root.viewHeight() - rule().height()) / 2);

    prepare();

    int result = 0; //d->subloop.exec();
    /// @todo Run an event loop here.

    finish(result);

    // No longer in the root.
    root.remove(*this);
    root.requestDraw();
    return result;
}

void DialogTextWidget::draw()
{
    Rectanglei pos = rule().recti().adjusted(Vec2i(-2, -1), Vec2i(2, 1));

    // Draw a background frame.
    targetCanvas().fill(pos, TextCanvas::AttribChar());
    targetCanvas().drawLineRect(pos);
}

bool DialogTextWidget::handleEvent(Event const &event)
{
    if (event.type() == Event::KeyPress)
    {
        KeyEvent const &ev = event.as<KeyEvent>();
        if (ev.key() == Key::Escape)
        {
            reject();
            return true;
        }
    }

    // All events not handled by children are eaten by the dialog.
    return true;
}

void DialogTextWidget::accept(int result)
{
//    if (d->subloop.isRunning())
    {
//        d->subloop.exit(result);
        DE_FOR_AUDIENCE2(Accept, i)
        {
            i->accepted(result);
        }
    }
}

void DialogTextWidget::reject(int result)
{
//    if (d->subloop.isRunning())
    {
//        d->subloop.exit(result);
        DE_FOR_AUDIENCE2(Reject, i)
        {
            i->rejected(result);
        }
    }
}

}} // namespace de::shell
