/** @file textrootwidget.cpp Text-based root widget.
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

#include "de/shell/TextRootWidget"
#include "de/shell/TextWidget"

namespace de {
namespace shell {

TextRootWidget::TextRootWidget(TextCanvas *cv) : RootWidget(), _canvas(cv), _drawRequest(false)
{
    DENG2_ASSERT(cv != 0);
    setViewSize(cv->size());
}

TextRootWidget::~TextRootWidget()
{
    delete _canvas;
}

TextCanvas &TextRootWidget::rootCanvas()
{
    return *_canvas;
}

void TextRootWidget::setViewSize(Vector2i const &viewSize)
{
    _canvas->resize(viewSize);
    RootWidget::setViewSize(viewSize);
}

TextWidget *TextRootWidget::focus() const
{
    return static_cast<TextWidget *>(RootWidget::focus());
}

void TextRootWidget::requestDraw()
{
    _drawRequest = true;
}

bool TextRootWidget::drawWasRequested() const
{
    return _drawRequest;
}

void TextRootWidget::draw()
{
    RootWidget::draw();
    if(focus())
    {
        _canvas->setCursorPosition(focus()->cursorPosition());
    }
    _canvas->show();
    _drawRequest = false;
}

} // namespace shell
} // namespace de
