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

    Instance(Public *i) : Base(i)
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
            setAuxBorderColorf(style().colors().colorf("accent"));
            aux->setTextColor("accent");
            break;

        case ButtonWidget::Hover:
            setAuxBorderColorf(style().colors().colorf("text"));
            aux->setTextColor("text");
            break;

        case ButtonWidget::Down:
            setAuxBorderColorf(style().colors().colorf(""),
                               style().colors().colorf("inverted.background"));
            aux->setTextColor("inverted.text");
            break;
        }
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
    setBackgroundColor("background");
    setTextColor("text");
    d->aux->setTextColor("accent");
    d->aux->setHoverTextColor("text", ButtonWidget::ReplaceColor);
    d->setAuxBorderColorf(style().colors().colorf("accent"));
}

void AuxButtonWidget::useInvertedStyle()
{
    setBackgroundColor("inverted.background");
    setTextColor("inverted.text");
    d->aux->setTextColor("inverted.text");
    d->aux->setHoverTextColor("inverted.text", ButtonWidget::ReplaceColor);
    d->setAuxBorderColorf(style().colors().colorf("inverted.text"));
}

} // namespace de
