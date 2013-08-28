/** @file sliderwidget.cpp  Slider to pick a value within a range.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/sliderwidget.h"

#include <de/Drawable>

using namespace de;
using namespace ui;

DENG2_PIMPL(SliderWidget)
{
    float value;
    Rangef range;
    float step;

    Instance(Public *i)
        : Base(i),
          value(0),
          range(0, 0),
          step(0)
    {}

    void glInit()
    {

    }

    void glDeinit()
    {

    }
};

SliderWidget::SliderWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{}

void SliderWidget::setRange(Rangei const &intRange, int step)
{

}

void SliderWidget::setRange(Rangef const &floatRange, float step)
{

}

void SliderWidget::setValue(float value)
{

}

Rangef SliderWidget::range() const
{
    return d->range;
}

float SliderWidget::value() const
{
    return d->value;
}

bool SliderWidget::handleEvent(Event const &event)
{


    return GuiWidget::handleEvent(event);
}

void SliderWidget::glInit()
{
    d->glInit();
}

void SliderWidget::glDeinit()
{
    d->glDeinit();
}
