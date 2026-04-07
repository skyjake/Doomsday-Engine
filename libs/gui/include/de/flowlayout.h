/** @file flowlayout.h  Widget layout for a row-based flow of widgets.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "ui/defs.h"
#include "de/guiwidget.h"
#include <de/isizerule.h>

namespace de {

/**
 * Widget layout for a row-based left-to-right sequence of widgets.
 *
 * Layouts are utilities that modify the placement of widgets. The layout
 * instance itself does not need to remain in memory -- widget rules are
 * modified immediately as the widgets are added to the layout.
 *
 * Widgets will reflow to the next row if they don't fit on the current
 * row. The row length must be provided as a separate independent rule.
 *
 * @note Because each position depends on the preceding ones and has the
 * option to either continue the row or break out to a new one, this creates
 * a lot of interdependent, branching rules. It is adviced that FlowLayout
 * is only used for a relatively small number of widgets in a sequence.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC FlowLayout : public ISizeRule
{
public:
    FlowLayout(const Rule &startX, const Rule &startY, const Rule &rowLength);

    void clear();

    void setStartX(const Rule &startX);
    void setStartY(const Rule &startY);

    inline FlowLayout &operator<<(GuiWidget &widget) { return append(widget); }
    inline FlowLayout &operator<<(const Rule &emptySpace) { return append(emptySpace); }

    FlowLayout &append(GuiWidget &widget);
    FlowLayout &append(const Rule &emptySpace);

    GuiWidgetList widgets() const;
    int           size() const;
    bool          isEmpty() const;

    // Implements ISizeRule.
    const Rule &width() const override;
    const Rule &height() const override;

private:
    DE_PRIVATE(d)
};

} // namespace de
