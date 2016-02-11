/** @file gamepanelbuttonwidget.cpp  Panel button for games.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/gamepanelbuttonwidget.h"
#include "ui/home/savelistwidget.h"
#include "ui/savedsessionlistdata.h"
#include "dd_main.h"

#include <doomsday/console/exec.h>
#include <de/CallbackAction>
#include <de/ChildWidgetOrganizer>

using namespace de;

DENG_GUI_PIMPL(GamePanelButtonWidget)
, public ChildWidgetOrganizer::IFilter
{
    Game const &game;
    SavedSessionListData const &savedItems;
    SaveListWidget *saves;
    ButtonWidget *playButton;

    Instance(Public *i, Game const &game, SavedSessionListData const &savedItems)
        : Base(i)
        , game(game)
        , savedItems(savedItems)
    {
        ButtonWidget *packages = new ButtonWidget;
        packages->setImage(style().images().image("package"));
        packages->setOverrideImageSize(style().fonts().font("default").height().value());
        packages->setSizePolicy(ui::Expand, ui::Expand);
        self.addButton(packages);

        playButton = new ButtonWidget;
        playButton->useInfoStyle();
        playButton->setImage(style().images().image("play"));
        playButton->setImageColor(style().colors().colorf("inverted.text"));
        playButton->setOverrideImageSize(style().fonts().font("default").height().value());
        playButton->setSizePolicy(ui::Expand, ui::Expand);
        playButton->setAction(new CallbackAction([this] () { playButtonPressed(); }));
        self.addButton(playButton);

        // List of saved games.
        saves = new SaveListWidget(self);
        saves->rule().setInput(Rule::Width, self.rule().width());
        saves->organizer().setFilter(*this);
        saves->setItems(savedItems);
        saves->margins().setZero().setLeft(self.icon().rule().width());

        self.panel().setContent(saves);
        self.panel().open();
    }

    void playButtonPressed()
    {
        BusyMode_FreezeGameForBusyMode();
        //ClientWindow::main().taskBar().close();
        // TODO: Emit a signal that hides the Home and closes the taskbar.

        // Switch the game.
        DoomsdayApp::app().changeGame(game, DD_ActivateGameWorker);

        if(saves->selectedPos() != ui::Data::InvalidPos)
        {
            // Load a saved game.
            auto const &saveItem = savedItems.at(saves->selectedPos());
            Con_Execute(CMDS_DDAY, ("loadgame " + saveItem.name() + " confirm").toLatin1(),
                        false, false);
        }
    }

//- ChildWidgetOrganizer::IFilter ---------------------------------------------

    bool isItemAccepted(ChildWidgetOrganizer const &organizer,
                        ui::Data const &data, ui::Data::Pos pos) const
    {
        // Only saved sessions for this game are to be included.
        auto const &item = data.at(pos).as<SavedSessionListData::SaveItem>();
        return item.gameId() == game.id();
    }
};

GamePanelButtonWidget::GamePanelButtonWidget(Game const &game, SavedSessionListData const &savedItems)
    : d(new Instance(this, game, savedItems))
{
    connect(d->saves, SIGNAL(selectionChanged(uint)), this, SLOT(saveSelected(uint)));
    connect(d->saves, SIGNAL(doubleClicked(uint)), this, SLOT(saveDoubleClicked(uint)));
    connect(this, SIGNAL(doubleClicked()), this, SLOT(play()));
}

void GamePanelButtonWidget::updateContent()
{
    enable(d->game.isPlayable());
    d->playButton->enable(isSelected());

    String meta = String::number(d->game.releaseDate().year());

    if(isSelected())
    {
        if(d->saves->selectedPos() != ui::Data::InvalidPos)
        {
            meta = tr("Restore saved game");
        }
        /*else if(d->saves->childCount() > 0)
        {
            meta = tr("%1 saved game%2")
                    .arg(d->saves->childCount())
                    .arg(d->saves->childCount() != 1? "s" : "");
        }*/
        else
        {
            meta = tr("Start new session");
        }
    }

    label().setText(String(_E(b) "%1\n" _E(l) "%2")
                    .arg(d->game.title())
                    .arg(meta));
}

void GamePanelButtonWidget::unselectSave()
{
    d->saves->clearSelection();
}

ButtonWidget &GamePanelButtonWidget::playButton()
{
    return *d->playButton;
}

void GamePanelButtonWidget::play()
{
    d->playButtonPressed();
}

void GamePanelButtonWidget::saveSelected(unsigned int /*savePos*/)
{
    updateContent();
}

void GamePanelButtonWidget::saveDoubleClicked(unsigned int savePos)
{
    d->saves->setSelectedPos(savePos);
    play();
}
