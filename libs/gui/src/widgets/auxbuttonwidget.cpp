/** @file auxbuttonwidget.cpp
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

#include "de/auxbuttonwidget.h"

namespace de {

DE_GUI_PIMPL(AuxButtonWidget)
, DE_OBSERVES(ButtonWidget, StateChange)
{
    ButtonWidget *aux;
    bool inverted;

    Impl(Public *i)
        : Base(i)
        , inverted(false)
    {
        self().add(aux = new ButtonWidget);
        aux->setFont("small");
        aux->setTextColor("text");
        aux->setSizePolicy(ui::Expand, ui::Fixed);
        const Rule &unit = rule(RuleBank::UNIT);
        aux->rule()
                .setInput(Rule::Right,  self().rule().right()  - unit)
                .setInput(Rule::Top,    self().rule().top()    + unit)
                .setInput(Rule::Bottom, self().rule().bottom() - unit);

        aux->audienceForStateChange() += this;

        self().margins().set("dialog.gap").setLeft("gap");
        self().margins().setRight(aux->rule().width() + rule("gap"));
    }

    void setAuxBorderColorf(const Vec4f &colorf)
    {
        aux->set(Background(Background::Rounded, colorf, 6));
    }

    void setAuxBorderColorf(const Vec4f &colorf, const Vec4f &bgColor)
    {
        aux->set(Background(bgColor, Background::Rounded, colorf, 6));
    }

    void buttonStateChanged(ButtonWidget &, ButtonWidget::State state)
    {
        switch (state)
        {
        case ButtonWidget::Up:
            if (!isInverted())
            {
                setAuxBorderColorf(style().colors().colorf("accent"));
                aux->setTextModulationColorf(style().colors().colorf("accent"));
            }
            else
            {
                setAuxBorderColorf(style().colors().colorf("inverted.accent"));
                aux->setTextModulationColorf(style().colors().colorf("inverted.accent"));
            }
            break;

        case ButtonWidget::Hover:
            if (!isInverted())
            {
                setAuxBorderColorf(style().colors().colorf("text"));
                aux->setTextModulationColorf(style().colors().colorf("text"));
            }
            else
            {
                setAuxBorderColorf(style().colors().colorf("inverted.text"));
                aux->setTextModulationColorf(style().colors().colorf("inverted.text"));
            }
            break;

        case ButtonWidget::Down:
            if (!isInverted())
            {
                setAuxBorderColorf(style().colors().colorf(""),
                                   style().colors().colorf("inverted.background"));
                aux->setTextModulationColorf(style().colors().colorf("inverted.text"));
            }
            else
            {
                setAuxBorderColorf(style().colors().colorf(""),
                                   style().colors().colorf("background"));
                aux->setTextModulationColorf(style().colors().colorf("text"));
            }
            break;
        }
    }

    bool isInverted() const
    {
        return inverted;
    }

    void updateStyle()
    {
        if (isInverted())
        {
            applyInvertedStyle();
        }
        else
        {
            applyNormalStyle();
        }
    }

    void applyNormalStyle()
    {
        aux->setHoverTextColor("text");
        buttonStateChanged(*aux, aux->state());
    }

    void applyInvertedStyle()
    {
        aux->setHoverTextColor("inverted.text");
        buttonStateChanged(*aux, aux->state());
    }
};

AuxButtonWidget::AuxButtonWidget(const String &name)
    : ButtonWidget(name), d(new Impl(this))
{
    useNormalStyle();
}

ButtonWidget &AuxButtonWidget::auxiliary()
{
    return *d->aux;
}

void AuxButtonWidget::useNormalStyle()
{
    useInfoStyle(false);

    d->inverted = false;
    d->updateStyle();
}

void AuxButtonWidget::invertStyle()
{
    useInfoStyle(!isUsingInfoStyle());
    d->inverted = !d->inverted;
    d->updateStyle();
}

void AuxButtonWidget::updateStyle()
{
    ButtonWidget::updateStyle();
    //d->updateStyle();
}

} // namespace de
