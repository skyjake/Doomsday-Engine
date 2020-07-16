/** @file gridpopupwidget.cpp
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

#include "de/gridpopupwidget.h"
#include "de/gridlayout.h"
#include "de/dialogcontentstylist.h"

namespace de {

DE_PIMPL_NOREF(GridPopupWidget)
{
    DialogContentStylist stylist;
    GuiWidget *container;
    GridLayout layout;
};

GridPopupWidget::GridPopupWidget(const String &name)
    : PopupWidget(name), d(new Impl)
{
    setOpeningDirection(ui::Up);
    setOutlineColor("popup.outline");

    d->container = new GuiWidget;
    setContent(d->container);

    d->stylist.setContainer(*d->container);

    // Initialize the layout.
    const Rule &gap = rule("gap");
    d->layout.setLeftTop(d->container->rule().left() + gap,
                         d->container->rule().top()  + gap);
    d->layout.setGridSize(2, 0);
    d->layout.setColumnAlignment(0, ui::AlignRight);
}

GridLayout &GridPopupWidget::layout()
{
    return d->layout;
}

LabelWidget &GridPopupWidget::addSeparatorLabel(const String &labelText)
{
//    auto *label = LabelWidget::newWithText(_E(D) + labelText, d->container);
//    label->setFont("separator.label");
//    label->margins().setTop("gap");
//    d->layout.setCellAlignment(Vector2i(0, d->layout.gridSize().y), ui::AlignLeft);
//    d->layout.append(*label, 2);
    return *LabelWidget::appendSeparatorWithText(labelText, d->container, &d->layout);
}

GridPopupWidget &GridPopupWidget::operator<<(GuiWidget *widget)
{
    d->container->add(widget);
    d->layout << *widget;
    return *this;
}

GridPopupWidget &GridPopupWidget::operator<<(const Rule &rule)
{
    d->layout << rule;
    return *this;
}

GridPopupWidget &GridPopupWidget::addSpanning(GuiWidget *widget, int cellSpan)
{
    d->container->add(widget);
    d->layout.setCellAlignment(Vec2i(0, d->layout.gridSize().y), ui::AlignLeft);
    d->layout.append(*widget, cellSpan);
    return *this;
}

void GridPopupWidget::commit()
{
    const Rule &gap = rule("gap");
    d->container->rule().setSize(d->layout.width()  + gap * 2,
                                 d->layout.height() + gap * 2);
}

} // namespace de
