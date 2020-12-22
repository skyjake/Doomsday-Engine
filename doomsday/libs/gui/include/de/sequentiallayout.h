/** @file sequentiallayout.h  Widget layout for a sequence of widgets.
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

#ifndef LIBAPPFW_SEQUENTIALLAYOUT_H
#define LIBAPPFW_SEQUENTIALLAYOUT_H

#include "ui/defs.h"
#include "guiwidget.h"
#include <de/isizerule.h>

namespace de {

/**
 * Widget layout for a sequence of widgets.
 *
 * Layouts are utilities that modify the placement of widgets. The layout
 * instance itself does not need to remain in memory -- widget rules are
 * modified immediately as the widgets are added to the layout.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC SequentialLayout : public ISizeRule
{
public:
    SequentialLayout(const Rule &startX,
                     const Rule &startY,
                     ui::Direction direction = ui::Down);

    void clear();

    void setStartX(const Rule &startX);
    void setStartY(const Rule &startY);

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
    void setOverrideWidth(const Rule &width);

    /**
     * Defines a height for all appended widgets. If the widget already has a
     * height defined, it will be overridden.
     *
     * @param height  Height for all widgets in the layout.
     */
    void setOverrideHeight(const Rule &height);

    SequentialLayout &operator << (GuiWidget &widget) { return append(widget); }
    SequentialLayout &operator << (const Rule &emptySpace) { return append(emptySpace); }

    enum AppendMode {
        UpdateMinorAxis,    ///< Layout total length on the minor axis is updated.
        IgnoreMinorAxis     ///< Does not affect layout total length on the minor axis.
    };

    SequentialLayout &append(GuiWidget &widget, AppendMode mode = UpdateMinorAxis);
    SequentialLayout &append(GuiWidget &widget, const Rule &spaceBefore, AppendMode mode = UpdateMinorAxis);
    SequentialLayout &append(const Rule &emptySpace);

    GuiWidgetList widgets() const;
    int size() const;
    bool isEmpty() const;

    const Rule &width() const override;
    const Rule &height() const override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_SEQUENTIALLAYOUT_H
