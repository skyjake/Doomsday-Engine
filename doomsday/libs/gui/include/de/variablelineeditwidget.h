/** @file variablelineeditwidget.h  Edits the text of a variable.
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

#ifndef LIBAPPFW_VARIABLELINEEDITWIDGET_H
#define LIBAPPFW_VARIABLELINEEDITWIDGET_H

#include <de/variable.h>

#include "de/lineeditwidget.h"

namespace de {

/**
 * Widget for editing the value of a text variable.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC VariableLineEditWidget : public LineEditWidget
{
public:
    /// Thrown when the variable is gone and someone tries to access it. @ingroup errors
    DE_ERROR(VariableMissingError);

public:
    VariableLineEditWidget(Variable &variable, const String &name = {});

    Variable &variable() const;

    void updateFromVariable();

protected:
    void setVariableFromWidget();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VARIABLELINEEDITWIDGET_H
