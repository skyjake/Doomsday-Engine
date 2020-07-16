/** @file textrootwidget.cpp Text-based root widget.
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

#include "de/term/textrootwidget.h"
#include "de/term/widget.h"

namespace de { namespace term {

TextRootWidget::TextRootWidget(TextCanvas *cv) : RootWidget(), _canvas(cv), _drawRequest(false)
{
    DE_ASSERT(cv != 0);
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

void TextRootWidget::setViewSize(const Size &viewSize)
{
    // Shouldn't go below 1 x 1.
    Size vs = viewSize.max(Size(1, 1));
    _canvas->resize(vs);
    RootWidget::setViewSize(vs);
}

term::Widget *TextRootWidget::focus() const
{
    return static_cast<term::Widget *>(RootWidget::focus());
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
    if (focus())
    {
        _canvas->setCursorPosition(focus()->cursorPosition());
    }
    _canvas->show();
    _drawRequest = false;
}

}} // namespace de::term
