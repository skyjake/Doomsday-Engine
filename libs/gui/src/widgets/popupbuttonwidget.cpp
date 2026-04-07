#include <utility>

#include <utility>

/** @file popupbuttonwidget.cpp
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/popupbuttonwidget.h"

namespace de {

DE_PIMPL(PopupButtonWidget)
, DE_OBSERVES(ButtonWidget, StateChange)
, DE_OBSERVES(ButtonWidget, Press)
, DE_OBSERVES(PanelWidget,  Close)
{
    SafeWidgetPtr<PopupWidget> pop;
    Constructor constructor;
    Opener opener;
    ui::Direction direction;
    bool popupWasOpenWhenButtonWentDown = false;

    Impl(Public *i) : Base(i) {}

    void buttonStateChanged(ButtonWidget &, State state)
    {
        if (state == Down)
        {
            popupWasOpenWhenButtonWentDown =
                    (pop && (pop->isOpen() || pop->isOpeningOrClosing()));
        }
    }

    void buttonPressed(ButtonWidget &)
    {
        if (!popupWasOpenWhenButtonWentDown)
        {
            if (constructor)
            {
                pop.reset(constructor(self()));
                self().add(pop);
                self().setOpeningDirection(direction);
                pop->setDeleteAfterDismissed(true);
            }
            if (opener)
            {
                opener(pop);
            }
            else if (pop)
            {
                pop->open();
            }

            if (auto *parentPop = self().findParentPopup())
            {
                parentPop->audienceForClose() += this;
            }
        }
    }

    void panelBeingClosed(PanelWidget &)
    {
        if (self().isOpen()) pop->close();
    }
};

PopupButtonWidget::PopupButtonWidget(const String &name)
    : ButtonWidget(name)
    , d(new Impl(this))
{
    audienceForStateChange() += d;
    audienceForPress()       += d;
}

void PopupButtonWidget::setPopup(PopupWidget &popup, ui::Direction openingDirection)
{
    d->pop.reset(&popup);
    d->constructor = Constructor();
    d->direction = openingDirection;
    setOpeningDirection(openingDirection);
}

void PopupButtonWidget::setOpener(Opener opener)
{
    d->opener = std::move(opener);
}

void PopupButtonWidget::setPopup(Constructor makePopup, ui::Direction openingDirection)
{
    d->pop.reset();
    d->direction = openingDirection;
    d->constructor = std::move(makePopup);
}

void PopupButtonWidget::setOpeningDirection(ui::Direction direction)
{
    d->direction = direction;
    if (d->pop)
    {
        d->pop->setAnchorAndOpeningDirection(rule(), direction);
    }
}

PopupWidget *PopupButtonWidget::popup() const
{
    return d->pop;
}

bool PopupButtonWidget::isOpen() const
{
    if (d->pop)
    {
        return d->pop->isOpen();
    }
    return false;
}

} // namespace de
