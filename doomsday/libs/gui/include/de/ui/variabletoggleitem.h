/** @file variabletoggleitem.h
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

#ifndef LIBAPPFW_UI_VARIABLETOGGLEITEM_H
#define LIBAPPFW_UI_VARIABLETOGGLEITEM_H

#include <de/variable.h>

#include "de/ui/item.h"

namespace de {
namespace ui {

/**
 * Represents a toggle for a boolean variable.
 *
 * @ingroup uidata
 */
class LIBGUI_PUBLIC VariableToggleItem : public Item
{
public:
    VariableToggleItem(const String &label, Variable &variable)
        : Item(ShownAsToggle, label), _var(variable) {}

    Variable &variable() const { return _var; }

private:
    Variable &_var;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_VARIABLETOGGLEITEM_H
