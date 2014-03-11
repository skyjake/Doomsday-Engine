/** @file auxbuttonwidget.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/AuxButtonWidget"

namespace de {

DENG_GUI_PIMPL(AuxButtonWidget)
, DENG2_OBSERVES(ButtonWidget, StateChange)
{
    ButtonWidget *aux;
    bool inverted;

    Instance(Public *i)
        : Base(i)
        , inverted(false)
    {
        self.add(aux = new ButtonWidget);
        aux->setFont("small");
        aux->setSizePolicy(ui::Expand, ui::Fixed);
        Rule const &unit = style().rules().rule("unit");
        aux->rule()
                .setInput(Rule::Right,  self.rule().right()  - unit)
                .setInput(Rule::Top,    self.rule().top()    + unit)
                .setInput(Rule::Bottom, self.rule().bottom() - unit);

        aux->audienceForStateChange() += this;

        self.margins().set("dialog.gap").setLeft("gap");
        self.margins().setRight(aux->rule().width() + style().rules().rule("gap"));
    }

    void setAuxBorderColorf(Vector4f const &colorf)
    {
        aux->set(Background(Background::Rounded, colorf, 6));
    }

    void setAuxBorderColorf(Vector4f const &colorf, Vector4f const &bgColor)
    {
        aux->set(Background(bgColor, Background::Rounded, colorf, 6));
    }

    void buttonStateChanged(ButtonWidget &, ButtonWidget::State state)
    {
        switch(state)
        {
        case ButtonWidget::Up:
            if(!isInverted())
            {
                setAuxBorderColorf(style().colors().colorf("accent"));
                aux->setTextColor("accent");
            }
            else
            {
                setAuxBorderColorf(style().colors().colorf("inverted.accent"));
                aux->setTextColor("inverted.accent");
            }
            break;

        case ButtonWidget::Hover:
            if(!isInverted())
            {
                setAuxBorderColorf(style().colors().colorf("text"));
                aux->setTextColor("text");
            }
            else
            {
                setAuxBorderColorf(style().colors().colorf("inverted.text"));
                aux->setTextColor("inverted.text");
            }
            break;

        case ButtonWidget::Down:
            if(!isInverted())
            {
                setAuxBorderColorf(style().colors().colorf(""),
                                   style().colors().colorf("inverted.background"));
                aux->setTextColor("inverted.text");
            }
            else
            {
                setAuxBorderColorf(style().colors().colorf(""),
                                   style().colors().colorf("background"));
                aux->setTextColor("text");
            }
            break;
        }
    }

    bool isInverted() const
    {
        return inverted ^ self.isUsingInfoStyle();
    }

    void updateStyle()
    {
        if(isInverted())
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
        self.setBackgroundColor("background");
        self.setTextColor("text");
        aux->setTextColor("accent");
        aux->setHoverTextColor("text", ButtonWidget::ReplaceColor);
        setAuxBorderColorf(style().colors().colorf("accent"));
    }

    void applyInvertedStyle()
    {
        self.setBackgroundColor("inverted.background");
        self.setTextColor("inverted.text");
        aux->setTextColor("inverted.text");
        aux->setHoverTextColor("inverted.text", ButtonWidget::ReplaceColor);
        setAuxBorderColorf(style().colors().colorf("inverted.text"));
    }
};

AuxButtonWidget::AuxButtonWidget(String const &name)
    : ButtonWidget(name), d(new Instance(this))
{
    useNormalStyle();
}

ButtonWidget &AuxButtonWidget::auxiliary()
{
    return *d->aux;
}

void AuxButtonWidget::useNormalStyle()
{
    d->inverted = false;
    d->updateStyle();
}

void AuxButtonWidget::useInvertedStyle()
{
    d->inverted = true;
    d->updateStyle();
}

void AuxButtonWidget::updateStyle()
{
    ButtonWidget::updateStyle();
    //d->updateStyle();
}

} // namespace de
