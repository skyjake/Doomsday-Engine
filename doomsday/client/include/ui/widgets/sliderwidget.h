/** @file sliderwidget.h  Slider to pick a value within a range.
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

#ifndef DENG_CLIENT_SLIDERWIDGET_H
#define DENG_CLIENT_SLIDERWIDGET_H

#include "GuiWidget"

#include <de/Range>

/**
 * Slider to pick a value within a range.
 *
 * The value can also be entered as text by right clicking on the slider.
 */
class SliderWidget : public GuiWidget
{
    Q_OBJECT

public:
    SliderWidget(de::String const &name = "");

    void setRange(de::Rangei const &intRange, int step = 0);
    void setRange(de::Rangef const &floatRange, float step = 0);
    void setPrecision(int precisionDecimals);
    void setValue(float value);

    de::Rangef range() const;
    float value() const;

    // Events.
    void viewResized();
    void update();
    void drawContent();
    bool handleEvent(de::Event const &event);

signals:
    void valueChanged(float value);
    void valueChangedByUser(float value);

protected:
    void glInit();
    void glDeinit();
    void updateStyle();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_SLIDERWIDGET_H
