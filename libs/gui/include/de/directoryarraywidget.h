/** @file directoryarraywidget.h  Widget for an array of native directories.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_DIRECTORYARRAYWIDGET_H
#define LIBAPPFW_DIRECTORYARRAYWIDGET_H

#include "de/variablearraywidget.h"

namespace de {

/**
 * Widget for choosing an array of native directories.
 *
 * @group guiWidgets
 */
class LIBGUI_PUBLIC DirectoryArrayWidget : public VariableArrayWidget
{
public:
    DirectoryArrayWidget(Variable &variable, const String &name = String());

protected:
    String labelForElement(const Value &value) const override;
    void elementCreated(LabelWidget &, const ui::Item &) override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_DIRECTORYARRAYWIDGET_H
