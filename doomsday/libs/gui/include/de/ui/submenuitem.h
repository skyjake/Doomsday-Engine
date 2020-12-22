/** @file submenuitem.h
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

#ifndef LIBAPPFW_UI_SUBMENUITEM_H
#define LIBAPPFW_UI_SUBMENUITEM_H

#include "../ui/defs.h"
#include "imageitem.h"
#include "listdata.h"

namespace de {
namespace ui {

/**
 * UI context item that contains items for a submenu.
 *
 * @ingroup uidata
 */
class LIBGUI_PUBLIC SubmenuItem : public ImageItem
{
public:
    SubmenuItem(const String &label, ui::Direction openingDirection)
        : ImageItem(ShownAsButton, label), _dir(openingDirection) {}

    SubmenuItem(const Image &image, const String &label, ui::Direction openingDirection)
        : ImageItem(ShownAsButton, image, label), _dir(openingDirection) {}

    Data &items() { return _items; }
    const Data &items() const { return _items; }
    ui::Direction openingDirection() const { return _dir; }

private:
    ListData _items;
    ui::Direction _dir;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_SUBMENUITEM_H
