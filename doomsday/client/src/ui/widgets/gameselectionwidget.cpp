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
#include "ui/widgets/gamefilterwidget.h"
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
, DENG2_OBSERVES(App, GameChange)
, public ChildWidgetOrganizer::IWidgetFactory
{    
    /// ActionItem with a Game member, for loading a particular game.
    struct GameItem : public ui::ActionItem {
        GameItem(Game const &gameRef, de::String const &label, RefArg<de::Action> action,
                 GameSelectionWidget &owner)
            : ui::ActionItem(label, action), game(gameRef), widget(owner) {
            setData(&gameRef);
        }
        String sortKey() const {
            // Sort by identity key.
            return game.identityKey();
        }
        Game const &game;
        GameSelectionWidget &widget;
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
    struct SubsetWidget
            : public FoldPanelWidget
            , DENG2_OBSERVES(ui::Data, Addition)
    {
        enum Type {
            NormalGames,
            MultiplayerGames
        };

        String titleText;
        Type type;
        MenuWidget *menu;
        LabelWidget *noGames;
        int numCols;

        SubsetWidget(Type selType, String const &headingText, GameSelectionWidget::Instance *owner)
            : titleText(headingText), type(selType), numCols(3)
        {           
            owner->self.add(makeTitle(headingText));
            title().setFont("title");
            title().setTextColor("inverted.text");
            title().setHoverTextColor("inverted.text", ButtonWidget::ReplaceColor);
            title().margins().setLeft("").setRight("");

            switch(type)
            {
            case NormalGames:
                menu = new MenuWidget;
                menu->organizer().setWidgetFactory(*owner);
                break;

            case MultiplayerGames:
                menu = new MPSelectionWidget;
                QObject::connect(menu, SIGNAL(gameSelected()), owner->thisPublic, SIGNAL(gameSessionSelected()));
                QObject::connect(menu, SIGNAL(availabilityChanged()), owner->thisPublic, SLOT(updateSubsetLayout()));
                break;
            }

            menu->items().audienceForAddition += this;

            setContent(menu);
            menu->enableScrolling(false);
            menu->margins().set("");
            menu->layout().setColumnPadding(owner->style().rules().rule("unit"));
            menu->rule().setInput(Rule::Width,
                                  owner->self.rule().width() -
                                  owner->self.margins().width());

            setColumns(3);

            // This will be shown if there are no games in the subset.
            noGames = LabelWidget::newWithText(_E(b) + tr("No games"), menu);
            noGames->margins()
                    .setTop   (style().rules().rule("gap") * 2)
                    .setBottom(noGames->margins().top());
            noGames->setFont("heading");
            noGames->setTextColor("inverted.text");
            noGames->setOpacity(.4f);
            noGames->hide();
        }

        void dataItemAdded(ui::Data::Pos, ui::Item const &)
        {
            // Time to get rid of the notice.
            noGames->hide();
        }

        void setColumns(int cols)
        {
            numCols = cols;

            // However, if the subset is empty, just use a single column for noGames.
            if(items().isEmpty())
            {
                cols = 1;
            }

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

        String textForTitle(bool whenOpen) const
        {
            if(whenOpen) return titleText;
            return QString("%1 (%2)").arg(titleText).arg(menu->items().size());
        }

        void preparePanelForOpening()
        {
            FoldPanelWidget::preparePanelForOpening();
            title().setText(textForTitle(true));
        }

        void panelClosing()
        {
            FoldPanelWidget::panelClosing();
            title().setText(textForTitle(false));
        }

        void updateTitleText()
        {
            title().setText(textForTitle(isOpen()));
        }

        void setTitleColor(DotPath const &colorId,
                           DotPath const &hoverColorId,
                           ButtonWidget::HoverColorMode mode)
        {
            title().setTextColor(colorId);
            title().setHoverTextColor(hoverColorId, mode);
            noGames->setTextColor(colorId);
        }
    };

    FIFO<Game> pendingGames;
    SequentialLayout superLayout;

    GameFilterWidget *filter;
    SubsetWidget *available;
    SubsetWidget *incomplete;
    SubsetWidget *multi;

    Instance(Public *i)
        : Base(i)
        , superLayout(i->contentRule().left(), i->contentRule().top(), ui::Down)
    {
        // Menu of available games.
        self.add(available = new SubsetWidget(SubsetWidget::NormalGames,
                App_GameLoaded()? tr("Switch Game") : tr("Available Games"), this));

        // Menu of incomplete games.
        self.add(incomplete = new SubsetWidget(SubsetWidget::NormalGames, tr("Games with Missing Resources"), this));

        // Menu of multiplayer games.
        self.add(multi = new SubsetWidget(SubsetWidget::MultiplayerGames, tr("Multiplayer Games"), this));

        self.add(filter = new GameFilterWidget);

        superLayout.setOverrideWidth(self.rule().width() - self.margins().width());

        updateSubsetLayout();

        App_Games().audienceForAddition += this;
        App::app().audienceForStartupComplete += this;
        App::app().audienceForGameChange += this;
    }

    ~Instance()
    {
        App_Games().audienceForAddition -= this;
        App::app().audienceForStartupComplete -= this;
        App::app().audienceForGameChange -= this;
    }

    void updateSubsetVisibility()
    {
        GameFilterWidget::Filter flt = filter->filter();

        bool const sp = flt.testFlag(GameFilterWidget::Singleplayer);
        bool const mp = flt.testFlag(GameFilterWidget::Multiplayer);

        available->show(sp);
        available->title().show(sp);

        incomplete->show(sp);
        incomplete->title().show(sp);

        multi->show(mp);
        multi->title().show(mp);
    }

    /**
     * Subsets are visible only when they have games in them. The title and content
     * of a subset are hidden when empty.
     */
    void updateSubsetLayout()
    {
        superLayout.clear();

        QList<SubsetWidget *> order;
        if(!App_GameLoaded())
        {
            order << available << multi << incomplete;
        }
        else
        {
            order << multi << available << incomplete;
        }

        updateSubsetVisibility();

        // Filter out the requested subsets.
        GameFilterWidget::Filter flt = filter->filter();
        if(!flt.testFlag(GameFilterWidget::Singleplayer))
        {
            order.removeOne(available);
            order.removeOne(incomplete);
        }
        if(!flt.testFlag(GameFilterWidget::Multiplayer))
        {
            order.removeOne(multi);
        }

        foreach(SubsetWidget *s, order)
        {
            s->updateTitleText();
            superLayout << s->title() << *s;

            // Show a notice when there are no games in the group.
            if(s->items().isEmpty())
            {
                // Go to one-column layout for the "no games" indicator.
                s->menu->setGridSize(1, ui::Filled, 1, ui::Expand);
                s->noGames->show();
            }
            else
            {
                // Restore the correct number of columns.
                s->setColumns(s->numCols);
                s->noGames->hide();
            }
        }

        self.setContentSize(superLayout.width(), superLayout.height());
    }

    void currentGameChanged(game::Game const &)
    {
        updateSubsetLayout();
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
        if(pendingGames.isEmpty()) return;

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

        sortGames();
        updateSubsetLayout();
    }

    struct LoadGameAction : public CommandAction
    {
        GameSelectionWidget &owner;

        LoadGameAction(String const &cmd, GameSelectionWidget &gameSel)
            : CommandAction(cmd)
            , owner(gameSel)
        {}

        void trigger()
        {
            emit owner.gameSessionSelected();
            CommandAction::trigger();
        }
    };

    ui::Item *makeItemForGame(Game &game)
    {
        String const idKey = game.identityKey();

        String label = String(_E(b) "%1" _E(.) "\n" _E(l)_E(D) "%2")
                .arg(game.title())
                .arg(idKey);

        GameItem *item = new GameItem(game, label, new LoadGameAction(String("load ") + idKey, self), self);

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
        w.loadButton().setAction(*it.action());
    }

    void appStartupCompleted()
    {
        updateGameAvailability();
    }

    static bool gameSorter(ui::Item const &a, ui::Item const &b)
    {
        GameItem const &x = a.as<GameItem>();
        GameItem const &y = b.as<GameItem>();

        switch(x.widget.d->filter->sortOrder())
        {
        case GameFilterWidget::SortByTitle:
            return x.label().compareWithoutCase(y.label()) < 0;

        case GameFilterWidget::SortByIdentityKey:
            return x.sortKey().compareWithoutCase(y.sortKey()) < 0;
        }

        return false;
    }

    void sortGames()
    {
        available->items().sort(gameSorter);
        incomplete->items().sort(gameSorter);
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

        sortGames();
        updateSubsetLayout();
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
    // By default attach the filter above the widget.
    d->filter->rule()
            .setInput(Rule::Width,  rule().width())
            .setInput(Rule::Bottom, rule().top())
            .setInput(Rule::Left,   rule().left());

    d->multi->open();
    if(!App_GameLoaded())
    {
        d->available->open();
        //d->incomplete->open();
    }

    // We want the full menu to be visible even when it doesn't fit the
    // designated area.
    unsetBehavior(ChildVisibilityClipping);
    unsetBehavior(ChildHitClipping);

    // Maybe there are games loaded already.
    d->addExistingGames();

    connect(d->filter, SIGNAL(sortOrderChanged()), this, SLOT(updateSort()));
    connect(d->filter, SIGNAL(filterChanged()), this, SLOT(updateSubsetLayout()));
}

void GameSelectionWidget::setTitleColor(DotPath const &colorId,
                                        DotPath const &hoverColorId,
                                        ButtonWidget::HoverColorMode mode)
{
    d->available ->setTitleColor(colorId, hoverColorId, mode);
    d->multi     ->setTitleColor(colorId, hoverColorId, mode);
    d->incomplete->setTitleColor(colorId, hoverColorId, mode);
}

void GameSelectionWidget::setTitleFont(DotPath const &fontId)
{
    d->available ->title().setFont(fontId);
    d->multi     ->title().setFont(fontId);
    d->incomplete->title().setFont(fontId);
}

GameFilterWidget &GameSelectionWidget::filter()
{
    return *d->filter;
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

void GameSelectionWidget::updateSubsetLayout()
{
    d->updateSubsetLayout();
}

void GameSelectionWidget::updateSort()
{
    d->sortGames();
}
