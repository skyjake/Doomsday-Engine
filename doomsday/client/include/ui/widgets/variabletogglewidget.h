/** @file variabletogglewidget.h  Toggles the value of a variable.
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

#ifndef DENG_CLIENT_VARIABLETOGGLEWIDGET_H
#define DENG_CLIENT_VARIABLETOGGLEWIDGET_H

#include "togglewidget.h"

#include <de/Variable>

/**
 * Widget for toggling the value of a variable.
 */
class VariableToggleWidget : public ToggleWidget
{
public:
    /// Thrown when the variable is gone and someone tries to access it. @ingroup errors
    DENG2_ERROR(VariableMissingError);

public:
    VariableToggleWidget(de::Variable &variable, de::String const &name = "");
    VariableToggleWidget(de::String const &styledText, de::Variable &variable, de::String const &name = "");

    de::Variable &variable() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_VARIABLETOGGLEWIDGET_H
