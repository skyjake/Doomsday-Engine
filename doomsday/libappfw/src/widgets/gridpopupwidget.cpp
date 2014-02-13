/** @file gridpopupwidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GridPopupWidget"
#include "de/GridLayout"
#include "de/DialogContentStylist"

namespace de {

DENG2_PIMPL_NOREF(GridPopupWidget)
{
    DialogContentStylist stylist;
    GuiWidget *container;
    GridLayout layout;
};

GridPopupWidget::GridPopupWidget(String const &name)
    : PopupWidget(name), d(new Instance)
{
    setOpeningDirection(ui::Up);

    d->container = new GuiWidget;
    setContent(d->container);

    d->stylist.setContainer(*d->container);

    // Initialize the layout.
    Rule const &gap = style().rules().rule("gap");
    d->layout.setLeftTop(d->container->rule().left() + gap,
                         d->container->rule().top()  + gap);
    d->layout.setGridSize(2, 0);
    d->layout.setColumnAlignment(0, ui::AlignRight);
}

GridLayout &GridPopupWidget::layout()
{
    return d->layout;
}

GridPopupWidget &GridPopupWidget::operator << (GuiWidget *widget)
{
    d->container->add(widget);
    d->layout << *widget;
    return *this;
}

GridPopupWidget &GridPopupWidget::operator << (Rule const &rule)
{
    d->layout << rule;
    return *this;
}

void GridPopupWidget::commit()
{
    Rule const &gap = style().rules().rule("gap");
    d->container->rule().setSize(d->layout.width()  + gap * 2,
                                 d->layout.height() + gap * 2);
}

} // namespace de
