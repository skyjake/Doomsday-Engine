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
#include <de/ConstantRule>

namespace de {
namespace shell {

struct LabelWidget::Instance
{
    TextCanvas::Char background;
    String label;
    LineWrapping wraps;
    TextCanvas::Char::Attribs attribs;
    Alignment align;
    bool vertExpand;
    ConstantRule *height;

    Instance() : align(0), vertExpand(false)
    {
        height = new ConstantRule(0);
    }

    ~Instance()
    {
        releaseRef(height);
    }

    void updateWraps(int width)
    {
        wraps.wrapTextToWidth(label, width);
        if(vertExpand) height->set(wraps.height());
    }
};

LabelWidget::LabelWidget(String const &name)
    : TextWidget(name), d(new Instance)
{}

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
    d->wraps.clear(); // updated later
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

void LabelWidget::setExpandsToFitLines(bool expand)
{
    d->vertExpand = expand;
    if(expand)
    {
        rule().setInput(RuleRectangle::Height, *d->height);
    }
    redraw();
}

String LabelWidget::label() const
{
    return d->label;
}

void LabelWidget::update()
{
    if(d->wraps.isEmpty())
    {
        d->updateWraps(de::floor(rule().width().value()));
    }
}

void LabelWidget::draw()
{
    Rectanglei pos = rule().recti();
    TextCanvas buf(pos.size());
    buf.clear(d->background);

    // Use the wrapped lines to determine width and height.
    DENG2_ASSERT(!d->wraps.isEmpty());
    Vector2i labelSize(d->wraps.width(), d->wraps.height());

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

    buf.drawWrappedText(labelPos, d->label, d->wraps, d->attribs, d->align);

    targetCanvas().draw(buf, pos.topLeft);
}

} // namespace shell
} // namespace de
