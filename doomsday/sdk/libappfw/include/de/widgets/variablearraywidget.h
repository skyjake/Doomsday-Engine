/** @file variablearraywidget.h  Widget for editing Variables with array values.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_VARIABLEARRAYWIDGET_H
#define LIBAPPFW_VARIABLEARRAYWIDGET_H

#include <de/Variable>

#include "../MenuWidget"

namespace de {

class ButtonWidget;

/**
 * Widget for editing Variables with array values.
 *
 * @ingroup guiWidgets
 */
class LIBAPPFW_PUBLIC VariableArrayWidget : public GuiWidget
{
    Q_OBJECT

public:
    /// Thrown when the variable is gone and someone tries to access it. @ingroup errors
    DENG2_ERROR(VariableMissingError);

public:
    VariableArrayWidget(Variable &variable, String const &name = String());

    Variable &variable() const;
    MenuWidget &elementsMenu();
    ButtonWidget &addButton();

    ui::Item *makeItem(Value const &value);

protected:
    virtual String labelForElement(Value const &value) const;

public slots:
    void updateFromVariable();

protected slots:
    void setVariableFromWidget();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VARIABLEARRAYWIDGET_H

