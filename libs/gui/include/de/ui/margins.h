/** @file margins.h  Margin rules for widgets.
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

#ifndef LIBAPPFW_UI_MARGINS_H
#define LIBAPPFW_UI_MARGINS_H

#include <de/rule.h>
#include <de/path.h>
#include <de/rectangle.h>

#include "defs.h"

namespace de {
namespace ui {

/**
 * Margin rules for a widget.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC Margins
{
public:
    DE_AUDIENCE(Change, void marginsChanged())

public:
    Margins(const String &defaultMargin = "gap");

    Margins &setLeft  (const DotPath &leftMarginId);
    Margins &setRight (const DotPath &rightMarginId);
    Margins &setTop   (const DotPath &topMarginId);
    Margins &setBottom(const DotPath &bottomMarginId);
    Margins &setLeftRight(const DotPath &marginId);
    Margins &setTopBottom(const DotPath &marginId);
    Margins &set      (ui::Direction dir, const DotPath &marginId);
    Margins &set      (const DotPath &marginId);

    Margins &setLeft  (const Rule &rule);
    Margins &setRight (const Rule &rule);
    Margins &setTop   (const Rule &rule);
    Margins &setBottom(const Rule &rule);
    Margins &setTopBottom(const Rule &rule) { return setTop(rule).setBottom(rule); }
    Margins &set      (ui::Direction dir, const Rule &rule);
    Margins &set      (const Rule &rule);
    Margins &setAll   (const Margins &margins);

    Margins &setZero();

    const Rule &left() const;
    const Rule &right() const;
    const Rule &top() const;
    const Rule &bottom() const;

    /**
     * The "width" of the margins is the sum of the left and right margins.
     */
    const Rule &width() const;

    /**
     * The "height" of the margins is the sim of the top and bottom margins.
     */
    const Rule &height() const;

    const Rule &margin(ui::Direction dir) const;

    /**
     * Returns all four margins as a vector. (x,y) is the left and top margins
     * and (z,w) is the right and bottom margins.
     */
    Vec4i toVector() const;

private:
    DE_PRIVATE(d)
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_MARGINS_H
