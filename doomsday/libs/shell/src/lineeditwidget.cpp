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

#include "de/shell/LineEditTextWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/KeyEvent"
#include "de/shell/Lexicon"
#include "de/shell/MonospaceLineWrapping"
#include <de/Rule>
#include <de/RuleRectangle>
#include <de/String>
#include <de/Log>

namespace de { namespace shell {

DE_PIMPL(LineEditTextWidget)
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

DE_AUDIENCE_METHOD(LineEditTextWidget, Enter)

LineEditTextWidget::LineEditTextWidget(de::String const &name)
    : TextWidget(name)
    , AbstractLineEditor(new MonospaceLineWrapping)
    , d(new Impl(*this))
{
    setBehavior(HandleEventsOnlyWhenFocused);

    // Widget's height is determined by the number of text lines.
    rule().setInput(Rule::Height, *d->height);
}

Vec2i LineEditTextWidget::cursorPosition() const
{
    de::Rectanglei pos = rule().recti();
    /// @todo Must calculate CharPos on the line.
    return pos.topLeft + Vec2i(prompt().size(), 0) + lineCursorPos();
}

void LineEditTextWidget::viewResized()
{
    updateLineWraps(RewrapNow);
}

void LineEditTextWidget::update()
{
    updateLineWraps(WrapUnlessWrappedAlready);
}

void LineEditTextWidget::draw()
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
    buf.drawWrappedText(Vec2i(prompt().size(), 0), txt, lineWraps(), attr);

    targetCanvas().draw(buf, pos.topLeft);
}

bool LineEditTextWidget::handleEvent(Event const &event)
{
    DE_ASSERT(event.type() == Event::KeyPress);

    // There are only key press events.
    KeyEvent const &ev = event.as<KeyEvent>();

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

    return TextWidget::handleEvent(event);
}

bool LineEditTextWidget::handleControlKey(Key key, const KeyModifiers &mods)
{
    if (AbstractLineEditor::handleControlKey(key, mods))
    {
        if (key == Key::Enter)
        {
            if (d->signalOnEnter)
            {
                DE_FOR_AUDIENCE2(Enter, i) i->enterPressed(text());
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

void LineEditTextWidget::setSignalOnEnter(int enterSignal)
{
    d->signalOnEnter = enterSignal;
}

int LineEditTextWidget::maximumWidth() const
{
    return int(rule().recti().width()) - int(prompt().size()) - 1;
}

void LineEditTextWidget::numberOfLinesChanged(int lineCount)
{
    d->height->set(lineCount);
}

void LineEditTextWidget::contentChanged()
{
    if (hasRoot())
    {
        updateLineWraps(RewrapNow);
    }
    redraw();
}

void LineEditTextWidget::cursorMoved()
{
    redraw();
}

}} // namespace de::shell
