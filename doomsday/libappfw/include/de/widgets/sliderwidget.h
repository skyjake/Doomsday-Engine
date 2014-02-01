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

#ifndef LIBAPPFW_SLIDERWIDGET_H
#define LIBAPPFW_SLIDERWIDGET_H

#include "../GuiWidget"

#include <de/Range>

namespace de {

/**
 * Slider to pick a value within a range.
 *
 * The value can also be entered as text by right clicking on the slider.
 */
class LIBAPPFW_PUBLIC SliderWidget : public GuiWidget
{
    Q_OBJECT

public:
    SliderWidget(String const &name = "");

    void setRange(Rangei const &intRange, int step = 1);
    void setRange(Rangef const &floatRange, float step = 0);
    void setRange(Ranged const &doubleRange, ddouble step = 0);
    void setPrecision(int precisionDecimals);
    void setValue(ddouble value);

    void setMinLabel(String const &labelText);
    void setMaxLabel(String const &labelText);

    /**
     * Displayed values are multiplied by this factor when displayed.
     * Does not affect the real value of the slider.
     *
     * @param factor  Display multiplier.
     */
    void setDisplayFactor(ddouble factor);

    Ranged range() const;
    ddouble value() const;
    int precision() const;
    ddouble displayFactor() const;

    // Events.
    void viewResized();
    void update();
    void drawContent();
    bool handleEvent(Event const &event);

public slots:
    void setValueFromText(QString text);

signals:
    void valueChanged(double value);
    void valueChangedByUser(double value);

protected:
    void glInit();
    void glDeinit();
    void updateStyle();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_SLIDERWIDGET_H
