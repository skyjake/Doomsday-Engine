/** @file lineeditwidget.cpp  Widget for word-wrapped text editing.
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

#include "de/shell/LineEditWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/KeyEvent"
#include "de/shell/Lexicon"
#include "de/shell/MonospaceLineWrapping"
#include <de/Rule>
#include <de/RuleRectangle>
#include <de/String>
#include <de/Log>
#include <QSet>

namespace de {
namespace shell {

DENG2_PIMPL(LineEditWidget)
{
    bool signalOnEnter;
    ConstantRule *height; ///< As rows.

    Instance(Public &i)
        : Base(i),
          signalOnEnter(true)
    {
        // Initial height of the command line (1 row).
        height = new ConstantRule(1);
    }

    ~Instance()
    {
        releaseRef(height);
    }
};

LineEditWidget::LineEditWidget(de::String const &name)
    : TextWidget(name),
      AbstractLineEditor(new MonospaceLineWrapping),
      d(new Instance(*this))
{
    setBehavior(HandleEventsOnlyWhenFocused);

    // Widget's height is determined by the number of text lines.
    rule().setInput(Rule::Height, *d->height);
}

Vector2i LineEditWidget::cursorPosition() const
{
    de::Rectanglei pos = rule().recti();
    return pos.topLeft + Vector2i(prompt().size(), 0) + lineCursorPos();
}

void LineEditWidget::viewResized()
{
    updateLineWraps(RewrapNow);
}

void LineEditWidget::update()
{
    updateLineWraps(WrapUnlessWrappedAlready);
}

void LineEditWidget::draw()
{
    Rectanglei pos = rule().recti();

    // Temporary buffer for drawing.
    TextCanvas buf(pos.size());

    TextCanvas::Char::Attribs attr =
            (hasFocus()? TextCanvas::Char::Reverse : TextCanvas::Char::DefaultAttributes);
    buf.clear(TextCanvas::Char(' ', attr));

    buf.drawText(Vector2i(0, 0), prompt(), attr | TextCanvas::Char::Bold);

    // Underline the suggestion for completion.
    if(isSuggestingCompletion())
    {
        buf.setRichFormatRange(TextCanvas::Char::Underline, completionRange());
    }

    // Echo mode determines what we actually draw.
    String txt = text();
    if(echoMode() == PasswordEchoMode)
    {
        txt = String(txt.size(), '*');
    }
    buf.drawWrappedText(Vector2i(prompt().size(), 0), txt, lineWraps(), attr);

    targetCanvas().draw(buf, pos.topLeft);
}

bool LineEditWidget::handleEvent(Event const &event)
{
    // There are only key press events.
    DENG2_ASSERT(event.type() == Event::KeyPress);
    KeyEvent const &ev = static_cast<KeyEvent const &>(event);

    bool eaten = true;

    // Insert text?
    if(!ev.text().isEmpty())
    {
        insert(ev.text());
    }
    else
    {
        // Control character.
        eaten = handleControlKey(ev.key());
    }

    if(eaten) return true;

    return TextWidget::handleEvent(event);
}

bool LineEditWidget::handleControlKey(int qtKey)
{
    if(AbstractLineEditor::handleControlKey(qtKey))
    {
        if(qtKey == Qt::Key_Enter)
        {
            if(d->signalOnEnter)
            {
                emit enterPressed(text());
            }
            else
            {
                // The Enter will fall through to base class event processing.
                return false;
            }
        }
        return true; // Handled.
    }
    return false;
}

void LineEditWidget::setSignalOnEnter(int enterSignal)
{
    d->signalOnEnter = enterSignal;
}

int LineEditWidget::maximumWidth() const
{
    return int(rule().recti().width()) - int(prompt().size()) - 1;
}

void LineEditWidget::numberOfLinesChanged(int lineCount)
{
    d->height->set(lineCount);
}

void LineEditWidget::contentChanged()
{
    if(hasRoot())
    {
        updateLineWraps(RewrapNow);
    }
    redraw();
}

void LineEditWidget::cursorMoved()
{
    redraw();
}

} // namespace shell
} // namespace de
