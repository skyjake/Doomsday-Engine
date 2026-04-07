/** @file variablesliderwidget.h  Slider that changes a Variable.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_VARIABLESLIDERWIDGET_H
#define LIBAPPFW_VARIABLESLIDERWIDGET_H

#include <de/variable.h>

#include "de/sliderwidget.h"

namespace de {

/**
 * Widget for changing the value of a Variable using a slider.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC VariableSliderWidget : public SliderWidget
{
public:
    /// Thrown when the variable is gone and someone tries to access it. @ingroup errors
    DE_ERROR(VariableMissingError);

    enum ValueType { Number, Animation };

public:
    VariableSliderWidget(Variable &variable, const Ranged &range, ddouble step = 0,
                         const String &name = String());

    VariableSliderWidget(ValueType valueType, Variable &variable,
                         const Ranged &range, ddouble step = 0,
                         const String &name = String());

    Variable &variable() const;

    void updateFromVariable();

protected:
    void setVariableFromWidget();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VARIABLESLIDERWIDGET_H
