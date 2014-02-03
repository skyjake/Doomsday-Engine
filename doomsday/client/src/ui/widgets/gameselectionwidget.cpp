/** @file gameselectionwidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/gameselectionwidget.h"
#include "ui/widgets/gamesessionwidget.h"
#include "ui/widgets/mpselectionwidget.h"
#include "CommandAction"
#include "clientapp.h"
#include "games.h"
#include "dd_main.h"

#include <de/ui/ActionItem>
#include <de/MenuWidget>
#include <de/SequentialLayout>
#include <de/FoldPanelWidget>
#include <de/DocumentPopupWidget>
#include <de/SignalAction>
#include <de/FIFO>
#include <QMap>

#include "CommandAction"

using namespace de;

DENG_GUI_PIMPL(GameSelectionWidget)
, DENG2_OBSERVES(Games, Addition)
, DENG2_OBSERVES(App, StartupComplete)
, public ChildWidgetOrganizer::IWidgetFactory
{
    /// ActionItem with a Game member, for loading a particular game.
    struct GameItem : public ui::ActionItem {
        GameItem(Game const &gameRef, de::String const &label, de::Action *action)
            : ui::ActionItem(label, action), game(gameRef) {
            setData(&gameRef);
        }
        Game const &game;
    };

    struct GameWidget : public GameSessionWidget
    {
        Game const *game;

        GameWidget() : game(0) {}
        void updateInfoContent() {
            DENG2_ASSERT(game != 0);
            document().setText(game->description());
        }
    };

    /**
     * Foldable group of games.
     */
    struct Subset : public FoldPanelWidget
    {
        enum Type {
            NormalGames,
            MultiplayerGames
        };

        Type type;
        MenuWidget *menu;

        Subset(Type selType, String const &headingText, GameSelectionWidget::Instance *owner)
            : type(selType)
        {           
            owner->self.add(makeTitle(headingText));
            title().setFont("title");
            title().setTextColor("inverted.text");
            title().setAlignment(ui::AlignLeft);
            title().margins().setLeft("");

            switch(type)
            {
            case NormalGames:
                menu = new MenuWidget;
                menu->organizer().setWidgetFactory(*owner);
                break;

            case MultiplayerGames:
                menu = new MPSelectionWidget;
                break;
            }

            setContent(menu);
            menu->enableScrolling(false);
            menu->margins().set("");
            menu->layout().setColumnPadding(owner->style().rules().rule("unit"));
            menu->rule().setInput(Rule::Width,
                                  owner->self.rule().width() -
                                  owner->self.margins().width());

            setColumns(3);
        }

        void setColumns(int cols)
        {
            if(menu->layout().maxGridSize().x != cols)
            {
                menu->setGridSize(cols, ui::Filled, 0, ui::Expand);
            }
        }

        ui::Data &items()
        {
            return menu->items();
        }

        GameWidget &gameWidget(ui::Item const &item)
        {
            return menu->itemWidget<GameWidget>(item);
        }
    };

    FIFO<Game> pendingGames;
    SequentialLayout superLayout;

    Subset *available;
    Subset *incomplete;
    Subset *multi;

    Instance(Public *i)
        : Base(i)
        , superLayout(i->contentRule().left(), i->contentRule().top(), ui::Down)
    {
        // Menu of available games.
        self.add(available = new Subset(Subset::NormalGames,
                                        tr("Available Games"), this));

        // Menu of incomplete games.
        self.add(incomplete = new Subset(Subset::NormalGames,
                                         tr("Games with Missing Resources"), this));

        // Menu of multiplayer games.
        self.add(multi = new Subset(Subset::MultiplayerGames,
                                    tr("Multiplayer Games"), this));

        superLayout.setOverrideWidth(self.rule().width() - self.margins().width());

        available->title().margins().setTop("");

        superLayout << available->title()
                    << *available
                    << multi->title()
                    << *multi
                    << incomplete->title()
                    << *incomplete;

        self.setContentSize(superLayout.width(), superLayout.height());

        App_Games().audienceForAddition += this;
        App::app().audienceForStartupComplete += this;
    }

    ~Instance()
    {
        App_Games().audienceForAddition -= this;
        App::app().audienceForStartupComplete -= this;
    }

    void gameAdded(Game &game)
    {
        // Called from a non-UI thread.
        pendingGames.put(&game);
    }

    void addExistingGames()
    {
        for(int i = 0; i < App_Games().count(); ++i)
        {
            gameAdded(App_Games().byIndex(i));
        }
    }

    void addPendingGames()
    {
        while(Game *game = pendingGames.take())
        {
            if(game->allStartupFilesFound())
            {
                ui::Item *item = makeItemForGame(*game);
                available->items().append(item);
                available->gameWidget(*item).loadButton().enable();
            }
            else
            {
                incomplete->items().append(makeItemForGame(*game));
            }
        }
    }

    ui::Item *makeItemForGame(Game &game)
    {
        String const idKey = game.identityKey();

        CommandAction *loadAction = new CommandAction(String("load ") + idKey);
        String label = String(_E(b) "%1" _E(.) /*_E(s)_E(C) " %2\n" _E(.)_E(.)*/ "\n"
                              _E(l)_E(D) "%2")
                .arg(game.title())
                //.arg(game.author())
                .arg(idKey);

        GameItem *item = new GameItem(game, label, loadAction);

        /// @todo The name of the plugin should be accessible via the plugin loader.
        String plugName;
        if(idKey.contains("heretic"))
        {
            plugName = "libheretic";
        }
        else if(idKey.contains("hexen"))
        {
            plugName = "libhexen";
        }
        else
        {
            plugName = "libdoom";
        }
        if(style().images().has(game.logoImageId()))
        {
            item->setImage(style().images().image(game.logoImageId()));
        }

        return item;
    }

    GuiWidget *makeItemWidget(ui::Item const &, GuiWidget const *)
    {
        return new GameWidget;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item)
    {
        GameWidget &w = widget.as<GameWidget>();
        GameItem const &it = item.as<GameItem>();

        w.game = &it.game;

        w.loadButton().setImage(it.image());
        w.loadButton().setText(it.label());
        w.loadButton().setAction(it.action()->duplicate());
    }

    void appStartupCompleted()
    {
        updateGameAvailability();
    }

    void updateGameAvailability()
    {
        // Available games.
        for(uint i = 0; i < available->items().size(); ++i)
        {
            GameItem const &item = available->items().at(i).as<GameItem>();
            if(item.game.allStartupFilesFound())
            {
                available->gameWidget(item).loadButton().enable();
            }
            else
            {
                incomplete->items().append(available->items().take(i--));
                incomplete->gameWidget(item).loadButton().disable();
            }
        }

        // Incomplete games.
        for(uint i = 0; i < incomplete->items().size(); ++i)
        {
            GameItem const &item = incomplete->items().at(i).as<GameItem>();
            if(item.game.allStartupFilesFound())
            {
                available->items().append(incomplete->items().take(i--));
                available->gameWidget(item).loadButton().enable();
            }
            else
            {
                incomplete->gameWidget(item).loadButton().disable();
            }
        }

        available->items().sort();
        incomplete->items().sort();
    }

    void updateLayoutForWidth(int width)
    {
        // If the view is too small, we'll want to reduce the number of items in the menu.
        int const maxWidth = style().rules().rule("gameselection.max.width").valuei();

        int suitable = clamp(1, 3 * width / maxWidth, 3);

        available->setColumns(suitable);
        incomplete->setColumns(suitable);
        multi->setColumns(suitable);
    }
};

GameSelectionWidget::GameSelectionWidget(String const &name)
    : ScrollAreaWidget(name), d(new Instance(this))
{
    d->available->open();
    d->multi->open();
    d->incomplete->open();

    // We want the full menu to be visible even when it doesn't fit the
    // designated area.
    unsetBehavior(ChildVisibilityClipping);

    // Maybe there are games loaded already.
    d->addExistingGames();
}

void GameSelectionWidget::setTitleColor(DotPath const &colorId)
{
    d->available->title().setTextColor(colorId);
    d->multi->title().setTextColor(colorId);
    d->incomplete->title().setTextColor(colorId);
}

void GameSelectionWidget::update()
{
    d->addPendingGames();

    ScrollAreaWidget::update();

    // Adapt grid layout for the widget width.
    Rectanglei rect;
    if(hasChangedPlace(rect))
    {
        d->updateLayoutForWidth(rect.width());
    }
}
