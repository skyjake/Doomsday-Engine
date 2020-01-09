/** @file groupwidget.h  GUI widget for grouping widgets.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_UI_GROUPWIDGET_H
#define LIBCOMMON_UI_GROUPWIDGET_H

#include <functional>
#include "hud/hudwidget.h"

/**
 * @defgroup groupWidgetFlags  Group Widget Flags
 */
///@{
#define UWGF_VERTICAL           0x0004
///@}

/**
 * @ingroup ui
 */
class GroupWidget : public HudWidget
{
public:
    GroupWidget(int player);
    virtual ~GroupWidget();

    int flags() const;
    void setFlags(int newFlags);

    order_t order() const;
    void setOrder(order_t newOrder);

    int padding() const;
    void setPadding(int newPadding);

    void tick(timespan_t elapsed);
    void updateGeometry();

public:
    /**
     * Returns the total number of child widgets in the group.
     */
    int childCount() const;

    /**
     * Append widget @a other to the list of child widgets in the group.
     */
    void addChild(HudWidget *other);

    /**
     * Iterate through the "child" widgets in the group, in insertion order.
     *
     * @param func  Callback to make for each widget.
     */
    de::LoopResult forAllChildren(const std::function<de::LoopResult (HudWidget &)>& func) const;

    /**
     * Empty the list of child widgets.
     */
    void clearAllChildren();

private:
    DE_PRIVATE(d)
};

#endif  // LIBCOMMON_UI_GROUPWIDGET_H
