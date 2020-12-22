/** @file documentpopupwidget.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/documentpopupwidget.h"
#include "de/buttonwidget.h"

namespace de {

DE_PIMPL_NOREF(DocumentPopupWidget)
{
    DocumentWidget *doc;
    ButtonWidget *  button = nullptr;
};

DocumentPopupWidget::DocumentPopupWidget(const String &name)
    : PopupWidget(name), d(new Impl)
{
    useInfoStyle();
    setContent(d->doc = new DocumentWidget);
}

DocumentPopupWidget::DocumentPopupWidget(ButtonWidget *actionButton, const String &name)
    : PopupWidget(name), d(new Impl)
{
    DE_ASSERT(actionButton);

    useInfoStyle();
    actionButton->useInfoStyle();

    GuiWidget *box = new GuiWidget;
    d->doc = new DocumentWidget;
    box->add(d->doc);
    box->add(actionButton);
    actionButton->setSizePolicy(ui::Expand, ui::Expand);

    const Rule &gap = rule("gap");

    box->rule()
            .setInput(Rule::Width,  d->doc->rule().width())
            .setInput(Rule::Height, d->doc->rule().height() +
                                    actionButton->rule().height() +
                                    gap);
    d->doc->rule()
            .setInput(Rule::Left,  box->rule().left())
            .setInput(Rule::Right, box->rule().right())
            .setInput(Rule::Top,   box->rule().top());
    actionButton->rule()
            .setInput(Rule::Right, box->rule().right() - gap)
            .setInput(Rule::Top,   d->doc->rule().bottom());

    setContent(box);
}

void DocumentPopupWidget::setPreferredHeight(const Rule &preferredHeight)
{
    d->doc->rule().setInput(Rule::Height,
                OperatorRule::minimum(preferredHeight,
                                      d->doc->contentRule().height() +
                                      d->doc->margins().height()));
}

void DocumentPopupWidget::setPreferredHeight(const Rule &preferredHeight, const Rule &maxHeight)
{
    d->doc->rule().setInput(Rule::Height,
            OperatorRule::minimum(
                OperatorRule::minimum(preferredHeight,
                                      d->doc->contentRule().height() +
                                      d->doc->margins().height()),
                maxHeight));
}

DocumentWidget &DocumentPopupWidget::document()
{
    return *d->doc;
}

const DocumentWidget &DocumentPopupWidget::document() const
{
    return *d->doc;
}

ButtonWidget *DocumentPopupWidget::button()
{
    return d->button;
}

} // namespace de
