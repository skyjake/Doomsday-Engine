/** @file variablechoicewidget.h  Choice widget for Variable values.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_VARIABLECHOICEWIDGET_H
#define DENG_CLIENT_VARIABLECHOICEWIDGET_H

#include <de/Variable>

#include "choicewidget.h"

/**
 * Widget for choosing the value of a variable from a set of possible values.
 */
class VariableChoiceWidget : public ChoiceWidget
{
    Q_OBJECT

public:
    /// Thrown when the variable is gone and someone tries to access it. @ingroup errors
    DENG2_ERROR(VariableMissingError);

public:
    VariableChoiceWidget(de::Variable &variable, de::String const &name = "");

    de::Variable &variable() const;

public slots:
    void updateFromVariable();

protected slots:
    void setVariableFromWidget();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_VARIABLECHOICEWIDGET_H
