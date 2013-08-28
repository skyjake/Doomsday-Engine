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

#ifndef DENG_CLIENT_UI_MARGINS_H
#define DENG_CLIENT_UI_MARGINS_H

#include <de/Rule>
#include <de/DotPath>
#include <de/Rectangle>

#include "../uidefs.h"

namespace ui {

/**
 * Margin rules for a widget.
 */
class Margins
{
public:
    DENG2_DEFINE_AUDIENCE(Change, void marginsChanged())

public:
    Margins(de::String const &defaultMargin = "gap");

    void setLeft  (de::DotPath const &leftMarginId);
    void setRight (de::DotPath const &rightMarginId);
    void setTop   (de::DotPath const &topMarginId);
    void setBottom(de::DotPath const &bottomMarginId);
    void set      (ui::Direction dir, de::DotPath const &marginId);
    void set      (de::DotPath const &marginId);

    void setLeft  (de::Rule const &rule);
    void setRight (de::Rule const &rule);
    void setTop   (de::Rule const &rule);
    void setBottom(de::Rule const &rule);
    void set      (ui::Direction dir, de::Rule const &rule);
    void set      (de::Rule const &rule);

    de::Rule const &left() const;
    de::Rule const &right() const;
    de::Rule const &top() const;
    de::Rule const &bottom() const;

    /**
     * The "width" of the margins is the sum of the left and right margins.
     */
    de::Rule const &width() const;

    /**
     * The "height" of the margins is the sim of the top and bottom margins.
     */
    de::Rule const &height() const;

    de::Rule const &margin(ui::Direction dir) const;

    /**
     * Returns all four margins as a vector. (x,y) is the left and top margins
     * and (z,w) is the right and bottom margins.
     */
    de::Vector4i toVector() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace ui

#endif // DENG_CLIENT_UI_MARGINS_H
