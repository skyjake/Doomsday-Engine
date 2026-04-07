/** @file variabletogglewidget.h  Toggles the value of a variable.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_VARIABLETOGGLEWIDGET_H
#define LIBAPPFW_VARIABLETOGGLEWIDGET_H

#include <de/variable.h>

#include "de/togglewidget.h"

namespace de {

/**
 * Widget for toggling the value of a variable.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC VariableToggleWidget : public ToggleWidget
{
public:
    /// Thrown when the variable is gone and someone tries to access it. @ingroup errors
    DE_ERROR(VariableMissingError);

public:
    VariableToggleWidget(Variable &variable, const String &name = String());
    VariableToggleWidget(const String &styledText, Variable &variable, const String &name = String());

    Variable &variable() const;

    void setActiveValue(double val);
    void setInactiveValue(double val);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VARIABLETOGGLEWIDGET_H
