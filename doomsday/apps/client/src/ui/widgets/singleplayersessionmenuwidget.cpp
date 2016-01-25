/** @file singleplayersessionmenuwidget.cpp
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

#include "ui/widgets/singleplayersessionmenuwidget.h"
#include "dd_main.h"
#include "CommandAction"

#include <de/ui/ActionItem>
#include <de/FIFO>
#include <de/App>
#include <de/Loop>

using namespace de;

DENG_GUI_PIMPL(SingleplayerSessionMenuWidget)
, DENG2_OBSERVES(Games, Addition)
, DENG2_OBSERVES(Games, Readiness)
, DENG2_OBSERVES(DoomsdayApp, GameChange)
{
    /// ActionItem with a Game member, for loading a particular game.
    struct GameItem : public ui::ImageItem,
                      public SessionItem
    {
        Game &game;

        GameItem(Game &gameRef,
                 de::String const &label,
                 SingleplayerSessionMenuWidget &owner)
            : ui::ImageItem(ShownAsButton, label)
            , SessionItem(owner)
            , game(gameRef)
        {}

        String title() const { return game.title(); }
        String gameId() const { return game.id(); }
    };

    struct GameWidget : public GameSessionWidget
    {
        SingleplayerSessionMenuWidget::Instance *owner;
        Game *game = nullptr;

        GameWidget() : GameSessionWidget(PopupWithDataFileButton) {}

        void updateInfoContent()
        {
            DENG2_ASSERT(game != 0);
            document().setText(game->description());
        }

        void setDataFiles(StringList const &paths)
        {
            game->setUserFiles(paths);
            owner->updateItemLabels();
        }
    };

    Mode mode;
    FIFO<Game> pendingGames;
    LoopCallback mainCall;

    Instance(Public *i) : Base(i)
    {
        App_Games().audienceForAddition() += this;
        App_Games().audienceForReadiness() += this;
        DoomsdayApp::app().audienceForGameChange() += this;
    }

    ~Instance()
    {
        App_Games().audienceForAddition() -= this;
        App_Games().audienceForReadiness() -= this;
        DoomsdayApp::app().audienceForGameChange() -= this;
    }

    void gameAdded(Game &game)
    {
        // Called from a non-UI thread.
        pendingGames.put(&game);

        // Update from main thread later.
        mainCall.enqueue([this] () {
            addPendingGames();
            updateGameAvailability();
        });
    }

    void addExistingGames()
    {
        for(int i = 0; i < App_Games().count(); ++i)
        {
            gameAdded(App_Games().byIndex(i));
        }
    }

    bool shouldBeShown(Game const &game)
    {
        bool const isReady = game.allStartupFilesFound();
        return ((mode == ShowAvailableGames && isReady) ||
                (mode == ShowGamesWithMissingResources && !isReady));
    }

    void addPendingGames()
    {
        if(pendingGames.isEmpty()) return;

        while(Game *game = pendingGames.take())
        {
            ui::Item *item = makeItemForGame(*game);
            self.items() << item;

            updateWidgetWithGameStatus(*item);
        }

        self.sort();
        emit self.availabilityChanged();
    }

    String labelForGame(Game const &game)
    {
        String label = String(_E(b) "%1" _E(.) "\n" _E(l)_E(D) "%2")
                .arg(game.title())
                .arg(game.id());

        if(!game.userFiles().isEmpty())
        {
            label += _E(b) " +" + QString::number(game.userFiles().size());
        }
        return label;
    }

    ui::Item *makeItemForGame(Game &game)
    {
        GameItem *item = new GameItem(game, labelForGame(game), self);

        if(style().images().has(game.logoImageId()))
        {
            item->setImage(style().images().image(game.logoImageId()));
        }

        return item;
    }

    void updateItemLabels()
    {
        for(uint i = 0; i < self.items().size(); ++i)
        {
            GameItem &item = self.items().at(i).as<GameItem>();
            item.setLabel(labelForGame(item.game));
            updateWidgetAction(item);
        }
    }

    void updateWidgetAction(GameItem const &item)
    {
        self.itemWidget<GameSessionWidget>(item).
                setDataFileAction(item.game.userFiles().isEmpty()? GameSessionWidget::Select :
                                                                   GameSessionWidget::Clear);
    }

    void updateWidgetWithGameStatus(ui::Item const &menuItem)
    {
        GameItem const &item = menuItem.as<GameItem>();
        GameSessionWidget &w = self.itemWidget<GameSessionWidget>(item);

        w.show(shouldBeShown(item.game));

        bool const isCurrentLoadedGame = (&App_CurrentGame() == &item.game);

        // Can be loaded?
        w.loadButton().enable(item.game.allStartupFilesFound() && !isCurrentLoadedGame);

        updateWidgetAction(item);
    }

    void updateGameAvailability()
    {
        for(uint i = 0; i < self.items().size(); ++i)
        {
            updateWidgetWithGameStatus(self.items().at(i));
        }
        self.sort();
        emit self.availabilityChanged();
    }

    void gameReadinessUpdated()
    {
        updateGameAvailability();
    }

    void currentGameChanged(Game const &)
    {
        mainCall.enqueue([this] () { updateGameAvailability(); });
    }
};

SingleplayerSessionMenuWidget::SingleplayerSessionMenuWidget(Mode mode, String const &name)
    : SessionMenuWidget(name), d(new Instance(this))
{
    d->mode = mode;

    // Maybe there are games loaded already.
    d->addExistingGames();
}

SingleplayerSessionMenuWidget::Mode SingleplayerSessionMenuWidget::mode() const
{
    return d->mode;
}

Action *SingleplayerSessionMenuWidget::makeAction(ui::Item const &item)
{
    return new CommandAction("load " + item.as<Instance::GameItem>().gameId());
}

GuiWidget *SingleplayerSessionMenuWidget::makeItemWidget(ui::Item const &, GuiWidget const *)
{
    auto *gw = new Instance::GameWidget;
    gw->owner = d;
    return gw;
}

void SingleplayerSessionMenuWidget::updateItemWidget(GuiWidget &widget, de::ui::Item const &item)
{
    Instance::GameWidget &w = widget.as<Instance::GameWidget>();
    Instance::GameItem const &it = item.as<Instance::GameItem>();

    w.game = &it.game;

    w.loadButton().setImage(it.image());
    w.loadButton().setText(it.label());
}
