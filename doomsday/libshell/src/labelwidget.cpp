/** @file labelwidget.h  Widget for showing a label.
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

#include "de/shell/LabelWidget"
#include "de/shell/TextRootWidget"

namespace de {
namespace shell {

struct LabelWidget::Instance
{
    TextCanvas::Char background;
    String label;
    TextCanvas::Char::Attribs attribs;
    Alignment align;
};

LabelWidget::LabelWidget(String const &name)
    : TextWidget(name), d(new Instance)
{
}

LabelWidget::~LabelWidget()
{
    delete d;
}

void LabelWidget::setBackground(TextCanvas::Char const &background)
{
    d->background = background;
}

void LabelWidget::setLabel(String const &text, TextCanvas::Char::Attribs attribs)
{
    d->label   = text;
    d->attribs = attribs;
    redraw();
}

void LabelWidget::setAttribs(TextCanvas::Char::Attribs const &attribs)
{
    d->attribs = attribs;
    redraw();
}

void LabelWidget::setAlignment(Alignment align)
{
    d->align = align;
    redraw();
}

String LabelWidget::label() const
{
    return d->label;
}

void LabelWidget::draw()
{
    Rectanglei pos = rule().recti();
    TextCanvas buf(pos.size());

    buf.clear(d->background);

    Vector2i labelSize(d->label.size(), 1);

    // Determine position of the label based on alignment.
    Vector2i labelPos;
    if(d->align.testFlag(AlignRight))
    {
        labelPos.x = buf.width() - labelSize.x;
    }
    else if(!d->align.testFlag(AlignLeft))
    {
        labelPos.x = buf.width()/2 - labelSize.x/2;
    }
    if(d->align.testFlag(AlignBottom))
    {
        labelPos.y = buf.height() - labelSize.y;
    }
    else if(!d->align.testFlag(AlignTop))
    {
        labelPos.y = buf.height()/2 - labelSize.y/2;
    }

    buf.drawText(labelPos, d->label, d->attribs);

    targetCanvas().draw(buf, pos.topLeft);
}

} // namespace shell
} // namespace de
