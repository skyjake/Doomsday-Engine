/** @file sequentiallayout.h  Widget layout for a sequence of widgets.
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

#ifndef LIBAPPFW_SEQUENTIALLAYOUT_H
#define LIBAPPFW_SEQUENTIALLAYOUT_H

#include "../ui/defs.h"
#include "../GuiWidget"

namespace de {

/**
 * Widget layout for a sequence of widgets.
 *
 * Layouts are utilities that modify the placement of widgets. The layout
 * instance itself does not need to remain in memory -- widget rules are
 * modified immediately as the widgets are added to the layout.
 */
class LIBAPPFW_PUBLIC SequentialLayout
{
public:
    SequentialLayout(Rule const &startX,
                     Rule const &startY,
                     ui::Direction direction = ui::Down);

    void clear();

    /**
     * Sets the direction of the layout. The direction can only be changed
     * when the layout is empty.
     *
     * @param direction  New layout direction.
     */
    void setDirection(ui::Direction direction);

    ui::Direction direction() const;

    /**
     * Defines a width for all appended widgets. If the widget already has a
     * width defined, it will be overridden.
     *
     * @param width  Width for all widgets in the layout.
     */
    void setOverrideWidth(Rule const &width);

    /**
     * Defines a height for all appended widgets. If the widget already has a
     * height defined, it will be overridden.
     *
     * @param width  Height for all widgets in the layout.
     */
    void setOverrideHeight(Rule const &height);

    SequentialLayout &operator << (GuiWidget &widget) { return append(widget); }
    SequentialLayout &operator << (Rule const &emptySpace) { return append(emptySpace); }

    enum AppendMode {
        UpdateMinorAxis,    ///< Layout total length on the minor axis is updated.
        IgnoreMinorAxis     ///< Does not affect layou total length on the minor axis.
    };

    SequentialLayout &append(GuiWidget &widget, AppendMode mode = UpdateMinorAxis);
    SequentialLayout &append(GuiWidget &widget, Rule const &spaceBefore, AppendMode mode = UpdateMinorAxis);
    SequentialLayout &append(Rule const &emptySpace);

    WidgetList widgets() const;
    int size() const;
    bool isEmpty() const;

    Rule const &width() const;
    Rule const &height() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_SEQUENTIALLAYOUT_H
