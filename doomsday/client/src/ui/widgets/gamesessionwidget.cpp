/** @file gamesessionwidget.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/gamesessionwidget.h"

#include <de/App>
#include <de/DocumentPopupWidget>

using namespace de;

DENG2_PIMPL(GameSessionWidget)
, DENG2_OBSERVES(de::ButtonWidget, Press)
, DENG2_OBSERVES(App, GameUnload)
{
    ButtonWidget *load;
    ButtonWidget *info;
    DocumentPopupWidget *popup;

    Instance(Public *i) : Base(i)
    {
        self.add(load = new ButtonWidget);
        self.add(info = new ButtonWidget);

        load->disable();
        load->setBehavior(Widget::ContentClipping);
        load->setAlignment(ui::AlignLeft);
        load->setTextAlignment(ui::AlignRight);
        load->setTextLineAlignment(ui::AlignLeft);

        info->setWidthPolicy(ui::Expand);
        info->setAlignment(ui::AlignBottom);
        info->setText(_E(s)_E(B) + tr("..."));

        self.add(popup = new DocumentPopupWidget);
        popup->setAnchorAndOpeningDirection(info->rule(), ui::Up);
        popup->document().setMaximumLineWidth(popup->style().rules().rule("document.popup.width").valuei());
        info->audienceForPress() += this;

        App::app().audienceForGameUnload() += this;
    }

    ~Instance()
    {
        App::app().audienceForGameUnload() -= this;
    }

    void aboutToUnloadGame(game::Game const &)
    {
        popup->close(0);
    }

    void buttonPressed(ButtonWidget &bt)
    {
        /*
        // Show information about the game.
        popup->setAnchorAndOpeningDirection(
                    bt.rule(),
                    bt.rule().top().valuei() + bt.rule().height().valuei() / 2 <
                    bt.root().viewRule().height().valuei() / 2?
                        ui::Down : ui::Up);*/
        self.updateInfoContent();
        popup->open();
    }
};

GameSessionWidget::GameSessionWidget() : d(new Instance(this))
{
    Font const &font = style().fonts().font("default");
    rule().setInput(Rule::Height, OperatorRule::maximum(font.lineSpacing() * 3 +
                                                        font.height() + margins().height(),
                                                        d->load->contentHeight()));

    // Button for extra information.
    d->load->rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Top,    rule().top())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Right,  d->info->rule().left());
    d->info->rule()
            .setInput(Rule::Top,    rule().top())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, rule().bottom());
}

ButtonWidget &GameSessionWidget::loadButton()
{
    return *d->load;
}

ButtonWidget &GameSessionWidget::infoButton()
{
    return *d->info;
}

DocumentWidget &GameSessionWidget::document()
{
    return d->popup->document();
}

void GameSessionWidget::updateInfoContent()
{
    // overridden by derived classes
}
