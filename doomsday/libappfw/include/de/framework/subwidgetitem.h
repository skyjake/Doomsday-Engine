/** @file subwidgetitem.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_UI_SUBWIDGETITEM_H
#define LIBAPPFW_UI_SUBWIDGETITEM_H

#include "../ui/defs.h"
#include "../ui/Item"

#include <de/Image>

namespace de {

class PopupWidget;

namespace ui {

/**
 * UI context item that opens a widget as a popup.
 */
class LIBAPPFW_PUBLIC SubwidgetItem : public Item
{
public:
    typedef PopupWidget *(*WidgetConstructor)();

public:
    SubwidgetItem(String const &label, ui::Direction openingDirection,
                  WidgetConstructor constructor)
        : Item(ShownAsButton, label)
        , _constructor(constructor)
        , _dir(openingDirection) {}

    SubwidgetItem(Image const &image, String const &label, ui::Direction openingDirection,
                  WidgetConstructor constructor)
        : Item(ShownAsButton, label)
        , _constructor(constructor)
        , _dir(openingDirection)
        , _image(image) {}

    PopupWidget *makeWidget() const { return _constructor(); }
    ui::Direction openingDirection() const { return _dir; }
    Image image() const { return _image; }

private:
    WidgetConstructor _constructor;
    ui::Direction _dir;
    Image _image;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_SUBWIDGETITEM_H
