/** @file libshell/src/lineeditwidget.cpp  Widget for word-wrapped text editing.
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

#include "de/term/lineeditwidget.h"
#include "de/term/textrootwidget.h"
#include "de/term/keyevent.h"
#include "de/lexicon.h"
#include "de/monospacelinewrapping.h"
#include "de/rule.h"
#include "de/rulerectangle.h"
#include "de/string.h"
#include "de/log.h"

namespace de { namespace term {

DE_PIMPL(LineEditWidget)
{
    bool signalOnEnter;
    ConstantRule *height; ///< As rows.

    Impl(Public &i)
        : Base(i),
          signalOnEnter(true)
    {
        // Initial height of the command line (1 row).
        height = new ConstantRule(1);
    }

    ~Impl()
    {
        releaseRef(height);
    }

    DE_PIMPL_AUDIENCE(Enter)
};

DE_AUDIENCE_METHOD(LineEditWidget, Enter)

LineEditWidget::LineEditWidget(const de::String &name)
    : Widget(name)
    , AbstractLineEditor(new MonospaceLineWrapping)
    , d(new Impl(*this))
{
    setBehavior(HandleEventsOnlyWhenFocused);

    // Widget's height is determined by the number of text lines.
    rule().setInput(Rule::Height, *d->height);
}

Vec2i LineEditWidget::cursorPosition() const
{
    de::Rectanglei pos = rule().recti();
    // Calculate CharPos on the cursor's line.
    const auto linePos     = lineCursorPos();
    const auto curLineSpan = lineWraps().line(linePos.line);
    WrapWidth  x           = lineWraps().rangeWidth(curLineSpan.range.left(linePos.x));
    int        y           = linePos.line;
    return pos.topLeft + Vec2i(prompt().sizei(), 0) + Vec2i(x, y);
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
    using AChar = TextCanvas::AttribChar;

    // Temporary buffer for drawing.
    Rectanglei pos = rule().recti();
    TextCanvas buf(pos.size());

    AChar::Attribs attr = (hasFocus() ? AChar::Reverse : AChar::DefaultAttributes);
    buf.clear(AChar(' ', attr));

    buf.drawText(Vec2i(0, 0), prompt(), attr | AChar::Bold);

    // Underline the suggestion for completion.
    if (isSuggestingCompletion())
    {
        buf.setRichFormatRange(AChar::Underline, completionRange());
    }

    // Echo mode determines what we actually draw.
    String txt = text();
    if (echoMode() == PasswordEchoMode)
    {
        txt = String(txt.size(), '*');
    }
    buf.drawWrappedText(Vec2i(prompt().sizei(), 0), txt, lineWraps(), attr);

    targetCanvas().draw(buf, pos.topLeft);
}

bool LineEditWidget::handleEvent(const Event &event)
{
    DE_ASSERT(event.type() == Event::KeyPress);

    // There are only key press events.
    const KeyEvent &ev = event.as<KeyEvent>();

    bool eaten = true;

    // Insert text?
    if (!ev.text().isEmpty())
    {
        insert(ev.text());
    }
    else
    {
        // Control character.
        eaten = handleControlKey(ev.key());
    }

    if (eaten) return true;

    return Widget::handleEvent(event);
}

bool LineEditWidget::handleControlKey(Key key, const KeyModifiers &mods)
{
    if (AbstractLineEditor::handleControlKey(key, mods))
    {
        if (key == Key::Enter)
        {
            if (d->signalOnEnter)
            {
                DE_NOTIFY(Enter, i) i->enterPressed(text());
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
    if (hasRoot())
    {
        updateLineWraps(RewrapNow);
    }
    redraw();
}

void LineEditWidget::cursorMoved()
{
    redraw();
}

}} // namespace de::term
