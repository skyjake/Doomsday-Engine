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
#include "TextDrawable"

#include <de/Drawable>

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(SliderWidget)
{
    float value;
    Rangef range;
    float step;

    // Visualization.
    bool animating;
    Animation pos;
    int thickness;

    // GL objects.
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;

    Instance(Public *i)
        : Base(i),
          value(0),
          range(0, 0),
          step(0),
          animating(false),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
    {
        self.setFont("slider.label");

        updateStyle();
    }

    void updateStyle()
    {
        thickness = style().fonts().font("default").height().valuei();
    }

    void glInit()
    {
        DefaultVertexBuf *buf = new DefaultVertexBuf;
        drawable.addBuffer(buf);

        shaders().build(drawable.program(), "generic.textured.color_ucolor")
                << uMvpMatrix << uColor << uAtlas();
    }

    void glDeinit()
    {
        drawable.clear();
    }

    void setValue(float v)
    {
        // Round to nearest step.
        if(step > 0)
        {
            v = de::roundf((v - range.start) / step) * step;
        }

        v = range.clamp(v);

        if(!fequal(v, value))
        {
            value = v;

            animating = true;
            pos.setValue(value, 0.25);
            self.requestGeometry();

            emit self.valueChanged(v);
        }
    }
};

SliderWidget::SliderWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{}

void SliderWidget::setRange(Rangei const &intRange, int step)
{
    d->range = Rangef(intRange.start, intRange.end);
    d->step = step;

    d->setValue(d->value);
}

void SliderWidget::setRange(Rangef const &floatRange, float step)
{
    d->range = floatRange;
    d->step = step;

    d->setValue(d->value);
}

void SliderWidget::setValue(float value)
{
    d->setValue(value);
}

Rangef SliderWidget::range() const
{
    return d->range;
}

float SliderWidget::value() const
{
    return d->value;
}

void SliderWidget::viewResized()
{
    GuiWidget::viewResized();

    d->uMvpMatrix = root().projMatrix2D();
}

void SliderWidget::update()
{
    GuiWidget::update();

    if(d->animating)
    {
        requestGeometry();

        d->animating = !d->pos.done();
    }
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

void SliderWidget::updateStyle()
{
    d->updateStyle();
}
