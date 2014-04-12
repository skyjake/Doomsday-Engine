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
, DENG2_OBSERVES(Loop, Iteration) // deferred updates
, DENG2_OBSERVES(App, StartupComplete)
{
    /// ActionItem with a Game member, for loading a particular game.
    struct GameItem : public ui::ImageItem,
                      public SessionItem
    {
        Game const &game;

        GameItem(Game const &gameRef,
                 de::String const &label,
                 SingleplayerSessionMenuWidget &owner)
            : ui::ImageItem(ShownAsButton, label)
            , SessionItem(owner)
            , game(gameRef)
        {
            setData(&gameRef);
        }

        String title() const { return game.title(); }
        String gameIdentityKey() const { return game.identityKey(); }
    };

    struct GameWidget : public GameSessionWidget
    {
        Game const *game;

        GameWidget() : game(0) {}

        void updateInfoContent()
        {
            DENG2_ASSERT(game != 0);
            document().setText(game->description());
        }
    };

    Mode mode;
    FIFO<Game> pendingGames;

    Instance(Public *i) : Base(i)
    {
        App_Games().audienceForAddition() += this;
        App::app().audienceForStartupComplete() += this;
    }

    ~Instance()
    {
        Loop::appLoop().audienceForIteration() -= this;

        App_Games().audienceForAddition() -= this;
        App::app().audienceForStartupComplete() -= this;
    }

    void gameAdded(Game &game)
    {
        // Called from a non-UI thread.
        pendingGames.put(&game);

        // Update from main thread later.
        Loop::appLoop().audienceForIteration() += this;
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

    void loopIteration()
    {
        Loop::appLoop().audienceForIteration() -= this;
        addPendingGames();
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

    ui::Item *makeItemForGame(Game &game)
    {
        String const idKey = game.identityKey();

        String label = String(_E(b) "%1" _E(.) "\n" _E(l)_E(D) "%2")
                .arg(game.title())
                .arg(idKey);

        GameItem *item = new GameItem(game, label, self);

        if(style().images().has(game.logoImageId()))
        {
            item->setImage(style().images().image(game.logoImageId()));
        }

        return item;
    }

    void updateWidgetWithGameStatus(ui::Item const &menuItem)
    {
        GameItem const &item = menuItem.as<GameItem>();
        GameSessionWidget &w = self.itemWidget<GameSessionWidget>(item);

        w.show(shouldBeShown(item.game));

        bool const isCurrentLoadedGame = (&App_CurrentGame() == &item.game);

        // Can be loaded?
        w.loadButton().enable(item.game.allStartupFilesFound() && !isCurrentLoadedGame);
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

    void appStartupCompleted()
    {
        updateGameAvailability();
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
    return new CommandAction("load " + item.as<Instance::GameItem>().gameIdentityKey());
}

GuiWidget *SingleplayerSessionMenuWidget::makeItemWidget(ui::Item const &item, GuiWidget const *parent)
{
    return new Instance::GameWidget;
}

void SingleplayerSessionMenuWidget::updateItemWidget(GuiWidget &widget, de::ui::Item const &item)
{
    Instance::GameWidget &w = widget.as<Instance::GameWidget>();
    Instance::GameItem const &it = item.as<Instance::GameItem>();

    w.game = &it.game;

    w.loadButton().setImage(it.image());
    w.loadButton().setText(it.label());
}
