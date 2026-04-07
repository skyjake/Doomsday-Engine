/** @file variantactionitem.h
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

#ifndef LIBAPPFW_UI_VARIANTACTIONITEM_H
#define LIBAPPFW_UI_VARIANTACTIONITEM_H

#include "actionitem.h"

namespace de {
namespace ui {

/**
 * Action item that has an alternative text label and image.
 *
 * @ingroup uidata
 */
class LIBGUI_PUBLIC VariantActionItem : public ActionItem
{
public:
    VariantActionItem(const String &label   = "",
                      const String &label2  = "",
                      RefArg<Action> action = RefArg<Action>())
        : ActionItem(ShownAsButton | ActivationClosesPopup, label, action)
        , _label2(label2)
    {}

    VariantActionItem(Semantics semantics,
                      const String &label   = "",
                      const String &label2  = "",
                      RefArg<Action> action = RefArg<Action>())
        : ActionItem(semantics, label, action)
        , _label2(label2)
    {}

    VariantActionItem(Semantics semantics,
                      const DotPath &styleImageId,
                      const DotPath &styleImageId2,
                      const String &label   = "",
                      const String &label2  = "",
                      RefArg<Action> action = RefArg<Action>())
        : ActionItem(semantics, styleImageId, label, action)
        , _label2(label2)
        , _image2(styleImageId2)
    {}

    VariantActionItem(const DotPath &styleImageId,
                      const DotPath &styleImageId2,
                      const String &label   = "",
                      const String &label2  = "",
                      RefArg<Action> action = RefArg<Action>())
        : ActionItem(ShownAsButton | ActivationClosesPopup, styleImageId, label, action)
        , _label2(label2)
        , _image2(styleImageId2)
    {}

    String label(bool useVariant) const {
        return useVariant? _label2 : ActionItem::label();
    }
    const DotPath &styleImageId(bool useVariant) const {
        return useVariant? _image2 : ActionItem::styleImageId();
    }

private:
    String _label2;
    DotPath _image2;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_VARIANTACTIONITEM_H
