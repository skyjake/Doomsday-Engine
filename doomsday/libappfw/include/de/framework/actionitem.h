/** @file actionitem.h
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

#ifndef LIBAPPFW_UI_ACTIONITEM_H
#define LIBAPPFW_UI_ACTIONITEM_H

#include "item.h"

#include <de/Action>
#include <de/Image>

namespace de {
namespace ui {

/**
 * UI context item that represents a user action.
 */
class LIBAPPFW_PUBLIC ActionItem : public Item
{
public:
    ActionItem(de::String const &label = "", de::Action *action = 0)
        : Item(ShownAsButton | ActivationClosesPopup, label), _action(action) {}

    ActionItem(Semantics semantics, de::String const &label = "", de::Action *action = 0)
        : Item(semantics, label), _action(action) {}

    ActionItem(Semantics semantics, de::Image const &img, de::String const &label = "", de::Action *action = 0)
        : Item(semantics, label), _action(action), _image(img) {}

    ActionItem(de::Image const &img, de::String const &label = "", de::Action *action = 0)
        : Item(ShownAsButton | ActivationClosesPopup, label), _action(action), _image(img) {}

    de::Action *action() const { return _action.data(); } // ownership kept
    de::Image const &image() const { return _image; }

    void setAction(de::Action *action)
    {
        _action.reset(action);
        notifyChange();
    }

    void setImage(de::Image const &image)
    {
        _image = image;
        notifyChange();
    }

private:
    QScopedPointer<de::Action> _action;
    de::Image _image;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_ACTIONITEM_H
