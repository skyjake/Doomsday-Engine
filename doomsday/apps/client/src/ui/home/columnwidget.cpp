/** @file columnwidget.cpp  Home column.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/columnwidget.h"

#include <de/LabelWidget>
#include <de/Range>
#include <de/math.h>

#include <QColor>

using namespace de;

DENG_GUI_PIMPL(ColumnWidget)
{
    bool highlighted;
    LabelWidget *back;
    Vector4f bgColor;
    Rule const *maxContentWidth = nullptr;

    Instance(Public *i) : Base(i)
    {
        back = new LabelWidget;

        QColor bg;
        bg.setHsvF(de::frand(), .9, .5);
        bgColor = Vector4f(bg.redF(), bg.greenF(), bg.blueF(), 1);
    }

    ~Instance()
    {
        releaseRef(maxContentWidth);
    }
};

ColumnWidget::ColumnWidget(String const &name)
    : GuiWidget(name)
    , d(new Instance(this))
{
    changeRef(d->maxContentWidth, rule().width() - style().rules().rule("gap") * 2);

    d->back->rule().setRect(rule());
    add(d->back);
}

Rule const &ColumnWidget::maximumContentWidth() const
{
    return *d->maxContentWidth;
}

void ColumnWidget::setHighlighted(bool highlighted)
{
    d->highlighted = highlighted;

    d->back->set(Background(highlighted? d->bgColor :
                                        (d->bgColor * Vector4f(.5f, .5f, .5f, 1.f))));
}
