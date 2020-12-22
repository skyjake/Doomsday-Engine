/** @file actionitem.h
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

#ifndef LIBAPPFW_UI_ACTIONITEM_H
#define LIBAPPFW_UI_ACTIONITEM_H

#include "imageitem.h"

#include <de/action.h>
#include <de/callbackaction.h>
#include <de/image.h>

namespace de {
namespace ui {

/**
 * UI context item that represents a user action.
 *
 * @ingroup uidata
 */
class LIBGUI_PUBLIC ActionItem : public ImageItem
{
public:
    ActionItem(const String &label = "", RefArg<Action> action = RefArg<Action>())
        : ImageItem(ShownAsButton | ActivationClosesPopup, label)
        , _action(action.holdRef())
    {}

    ActionItem(const String &label,
               const std::function<void ()> &actionFunc)
        : ImageItem(ShownAsButton | ActivationClosesPopup, label)
        , _action(new CallbackAction(actionFunc)) {}

    ActionItem(Semantics semantics,
               const String &label,
               const std::function<void ()> &actionFunc)
        : ImageItem(semantics, label)
        , _action(new CallbackAction(actionFunc)) {}

    ActionItem(Semantics semantics,
               const String &label   = "",
               RefArg<Action> action = RefArg<Action>())
        : ImageItem(semantics, label)
        , _action(action.holdRef()) {}

    ActionItem(Semantics semantics,
               const Image &img,
               const String &label   = "",
               RefArg<Action> action = RefArg<Action>())
        : ImageItem(semantics, img, label)
        , _action(action.holdRef()) {}

    ActionItem(const Image &img,
               const String &label   = "",
               RefArg<Action> action = RefArg<Action>())
        : ImageItem(ShownAsButton | ActivationClosesPopup, img, label)
        , _action(action.holdRef()) {}

    ActionItem(const Image &img,
               const String &label,
               const std::function<void ()> &actionFunc)
        : ImageItem(ShownAsButton | ActivationClosesPopup, img, label)
        , _action(new CallbackAction(actionFunc)) {}

    ActionItem(Semantics semantics,
               const DotPath &styleImageId,
               const String &label   = "",
               RefArg<Action> action = RefArg<Action>())
        : ImageItem(semantics, styleImageId, label)
        , _action(action.holdRef()) {}

    ActionItem(const DotPath &styleImageId,
               const String &label   = "",
               RefArg<Action> action = RefArg<Action>())
        : ImageItem(ShownAsButton | ActivationClosesPopup, styleImageId, label)
        , _action(action.holdRef()) {}

    const Action *action() const { return _action; }

    void setAction(RefArg<Action> action)
    {
        _action.reset(action);
        notifyChange();
    }

private:
    AutoRef<Action> _action;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_ACTIONITEM_H
