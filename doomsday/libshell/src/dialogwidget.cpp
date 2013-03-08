/** @file dialogwidget.cpp  Base class for modal dialogs.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/shell/DialogWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/KeyEvent"
#include <QEventLoop>

namespace de {
namespace shell {

DENG2_PIMPL_NOREF(DialogWidget)
{
    QEventLoop subloop;
};

DialogWidget::DialogWidget(String const &name)
    : TextWidget(name), d(new Instance)
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
    root().setFocus(0);
}

int DialogWidget::exec(TextRootWidget &root)
{
    // The widget is added to the root temporarily (as top child).
    DENG2_ASSERT(!hasRoot());
    root.add(this);

    // Center the dialog.
    rule().setInput(Rule::Left,  (root.viewWidth()  - rule().width())  / 2)
          .setInput(Rule::Top,   (root.viewHeight() - rule().height()) / 2);

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
    Rectanglei pos = rule().recti().adjusted(Vector2i(-2, -1), Vector2i(2, 1));

    // Draw a background frame.
    targetCanvas().fill(pos, TextCanvas::Char());
    targetCanvas().drawLineRect(pos);
}

bool DialogWidget::handleEvent(Event const &event)
{
    if(event.type() == Event::KeyPress)
    {
        KeyEvent const &ev = static_cast<KeyEvent const &>(event);
        if(ev.key() == Qt::Key_Escape)
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
    if(d->subloop.isRunning())
    {
        d->subloop.exit(result);
        emit accepted(result);
    }
}

void DialogWidget::reject(int result)
{
    if(d->subloop.isRunning())
    {
        d->subloop.exit(result);
        emit rejected(result);
    }
}

} // namespace shell
} // namespace de
