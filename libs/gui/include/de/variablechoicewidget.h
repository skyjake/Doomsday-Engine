/** @file variablechoicewidget.h  Choice widget for Variable values.
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

#ifndef LIBAPPFW_VARIABLECHOICEWIDGET_H
#define LIBAPPFW_VARIABLECHOICEWIDGET_H

#include <de/variable.h>

#include "de/choicewidget.h"

namespace de {

/**
 * Widget for choosing the value of a variable from a set of possible values.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC VariableChoiceWidget : public ChoiceWidget
{
public:
    /// Thrown when the variable is gone and someone tries to access it. @ingroup errors
    DE_ERROR(VariableMissingError);

    enum VariableType { Number, Text };

public:
    VariableChoiceWidget(Variable &variable, VariableType variableType,
                         const String &name = {});

    Variable &variable() const;

    void updateFromVariable();

protected:
    void setVariableFromWidget();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VARIABLECHOICEWIDGET_H
