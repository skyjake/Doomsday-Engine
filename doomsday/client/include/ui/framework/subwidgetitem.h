/** @file subwidgetitem.h
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

#ifndef DENG_CLIENT_UI_SUBWIDGETITEM_H
#define DENG_CLIENT_UI_SUBWIDGETITEM_H

#include "../uidefs.h"
#include "item.h"

#include <de/Image>

class PopupWidget;

namespace ui {

/**
 * UI context item that opens a widget as a popup.
 */
class SubwidgetItem : public Item
{
public:
    typedef PopupWidget *(*WidgetConstructor)();

public:
    SubwidgetItem(de::String const &label, ui::Direction openingDirection,
                  WidgetConstructor constructor)
        : Item(ShownAsButton, label)
        , _constructor(constructor)
        , _dir(openingDirection) {}

    SubwidgetItem(de::Image const &image, de::String const &label, ui::Direction openingDirection,
                  WidgetConstructor constructor)
        : Item(ShownAsButton, label)
        , _constructor(constructor)
        , _dir(openingDirection)
        , _image(image) {}

    PopupWidget *makeWidget() const { return _constructor(); }
    ui::Direction openingDirection() const { return _dir; }
    de::Image image() const { return _image; }

private:
    WidgetConstructor _constructor;
    ui::Direction _dir;
    de::Image _image;
};

} // namespace ui

#endif // DENG_CLIENT_UI_SUBWIDGETITEM_H
