/** @file term/labelwidget.cpp  Widget for showing a label.
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

#include "de/term/labelwidget.h"
#include "de/term/textrootwidget.h"
#include "de/monospacelinewrapping.h"
#include "de/constantrule.h"

namespace de { namespace term {

DE_PIMPL_NOREF(LabelWidget)
{
    TextCanvas::AttribChar          background;
    String                          label;
    MonospaceLineWrapping           wraps;
    TextCanvas::AttribChar::Attribs attribs;
    Alignment                       align;
    bool                            vertExpand;
    ConstantRule *                  height;

    Impl() : align(0), vertExpand(false)
    {
        height = new ConstantRule(0);
    }

    ~Impl()
    {
        releaseRef(height);
    }

    void updateWraps(int width)
    {
        wraps.wrapTextToWidth(label, width);
        if (vertExpand) height->set(wraps.height());
    }
};

LabelWidget::LabelWidget(const String &name)
    : Widget(name), d(new Impl)
{}

void LabelWidget::setBackground(const TextCanvas::AttribChar &background)
{
    d->background = background;
}

void LabelWidget::setLabel(const String &text, TextCanvas::AttribChar::Attribs attribs)
{
    d->label   = text;
    d->attribs = std::move(attribs);
    d->wraps.clear(); // updated later
    redraw();
}

void LabelWidget::setAttribs(const TextCanvas::AttribChar::Attribs &attribs)
{
    d->attribs = attribs;
    redraw();
}

void LabelWidget::setBackgroundAttribs(const TextCanvas::AttribChar::Attribs &attribs)
{
    d->background.attribs = attribs;
    redraw();
}

TextCanvas::AttribChar::Attribs LabelWidget::attribs() const
{
    return d->attribs;
}

void LabelWidget::setAlignment(Alignment align)
{
    d->align = std::move(align);
    redraw();
}

void LabelWidget::setExpandsToFitLines(bool expand)
{
    d->vertExpand = expand;
    if (expand)
    {
        rule().setInput(Rule::Height, *d->height);
    }
    redraw();
}

String LabelWidget::label() const
{
    return d->label;
}

void LabelWidget::update()
{
    if (d->wraps.isEmpty())
    {
        d->updateWraps(rule().width().valuei());
    }
}

void LabelWidget::draw()
{
    Rectanglei pos = rule().recti();
    TextCanvas buf(pos.size());
    buf.clear(d->background);

    // Use the wrapped lines to determine width and height.
    DE_ASSERT(!d->wraps.isEmpty());
    Vec2i labelSize(d->wraps.width(), d->wraps.height());

    // Determine position of the label based on alignment.
    Vec2i labelPos;
    if (d->align.testFlag(AlignRight))
    {
        labelPos.x = buf.width() - labelSize.x;
    }
    else if (!d->align.testFlag(AlignLeft))
    {
        labelPos.x = buf.width()/2 - labelSize.x/2;
    }
    if (d->align.testFlag(AlignBottom))
    {
        labelPos.y = buf.height() - labelSize.y;
    }
    else if (!d->align.testFlag(AlignTop))
    {
        labelPos.y = buf.height()/2 - labelSize.y/2;
    }

    buf.drawWrappedText(labelPos, d->label, d->wraps, d->attribs, d->align);

    targetCanvas().draw(buf, pos.topLeft);
}

}} // namespace de::term
