/** @file subwidgetitem.h
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

#ifndef LIBAPPFW_UI_SUBWIDGETITEM_H
#define LIBAPPFW_UI_SUBWIDGETITEM_H

#include "../ui/defs.h"
#include "de/ui/imageitem.h"

#include <de/image.h>
#include <functional>

namespace de {

class PopupWidget;

namespace ui {

/**
 * UI context item that opens a widget as a popup.
 *
 * @ingroup uidata
 */
class LIBGUI_PUBLIC SubwidgetItem : public ImageItem
{
public:
    typedef std::function<PopupWidget *()> WidgetConstructor;

public:
    SubwidgetItem(const String &label, ui::Direction openingDirection,
                  WidgetConstructor constructor)
        : ImageItem(ShownAsButton, label)
        , _constructor(constructor)
        , _dir(openingDirection) {}

    SubwidgetItem(Semantics semantics, const String &label, ui::Direction openingDirection,
                  WidgetConstructor constructor)
        : ImageItem(semantics, label)
        , _constructor(constructor)
        , _dir(openingDirection) {}

    SubwidgetItem(const Image &image, const String &label, ui::Direction openingDirection,
                  WidgetConstructor constructor)
        : ImageItem(ShownAsButton, image, label)
        , _constructor(constructor)
        , _dir(openingDirection) {}

    SubwidgetItem(const Image &image, Semantics semantics, const String &label,
                  ui::Direction openingDirection, WidgetConstructor constructor)
        : ImageItem(semantics, image, label)
        , _constructor(constructor)
        , _dir(openingDirection)
    {}

    PopupWidget *makeWidget() const { return _constructor(); }
    ui::Direction openingDirection() const { return _dir; }

private:
    WidgetConstructor _constructor;
    ui::Direction _dir;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_SUBWIDGETITEM_H
