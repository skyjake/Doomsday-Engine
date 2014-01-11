/** @file margins.h  Margin rules for widgets.
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

#ifndef LIBAPPFW_UI_MARGINS_H
#define LIBAPPFW_UI_MARGINS_H

#include <de/Rule>
#include <de/DotPath>
#include <de/Rectangle>

#include "../ui/defs.h"

namespace de {
namespace ui {

/**
 * Margin rules for a widget.
 */
class LIBAPPFW_PUBLIC Margins
{
public:
    DENG2_DEFINE_AUDIENCE(Change, void marginsChanged())

public:
    Margins(String const &defaultMargin = "gap");

    void setLeft  (DotPath const &leftMarginId);
    void setRight (DotPath const &rightMarginId);
    void setTop   (DotPath const &topMarginId);
    void setBottom(DotPath const &bottomMarginId);
    void set      (ui::Direction dir, DotPath const &marginId);
    void set      (DotPath const &marginId);

    void setLeft  (Rule const &rule);
    void setRight (Rule const &rule);
    void setTop   (Rule const &rule);
    void setBottom(Rule const &rule);
    void set      (ui::Direction dir, Rule const &rule);
    void set      (Rule const &rule);
    void setAll   (Margins const &margins);

    Rule const &left() const;
    Rule const &right() const;
    Rule const &top() const;
    Rule const &bottom() const;

    /**
     * The "width" of the margins is the sum of the left and right margins.
     */
    Rule const &width() const;

    /**
     * The "height" of the margins is the sim of the top and bottom margins.
     */
    Rule const &height() const;

    Rule const &margin(ui::Direction dir) const;

    /**
     * Returns all four margins as a vector. (x,y) is the left and top margins
     * and (z,w) is the right and bottom margins.
     */
    Vector4i toVector() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_MARGINS_H
