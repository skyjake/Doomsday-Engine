/** @file gameselectionwidget.cpp
 *
 * @authors Copyright © 2013-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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
#include "ui/widgets/gamefilterwidget.h"
#include "ui/widgets/singleplayersessionmenuwidget.h"
#include "ui/widgets/mpsessionmenuwidget.h"
#include "ui/widgets/savedsessionmenuwidget.h"
#include "CommandAction"
#include "clientapp.h"
#include "games.h"
#include "dd_main.h"

#include <de/MenuWidget>
#include <de/SequentialLayout>
#include <de/FoldPanelWidget>
#include <de/DocumentPopupWidget>
#include <de/SignalAction>
#include <QMap>

#include "CommandAction"

using namespace de;

DENG_GUI_PIMPL(GameSelectionWidget)
, DENG2_OBSERVES(App, GameChange)
{
    /**
     * Foldable group of games.
     */
    struct SubsetWidget
            : public FoldPanelWidget
            , DENG2_OBSERVES(ui::Data, Addition)
    {
        enum Type {
            AvailableGames,
            IncompleteGames,
            MultiplayerGames,
            SavedGames
        };

        String titleText;
        Type type;
        SessionMenuWidget *menu;
        LabelWidget *noGames;
        int numCols;

        SubsetWidget(String const &widgetName, Type selType,
                     String const &headingText, GameSelectionWidget::Instance *owner)
            : FoldPanelWidget(widgetName)
            , titleText(headingText)
            , type(selType)
            , numCols(3)
        {           
            owner->self.add(makeTitle(headingText));
            title().setFont("title");
            title().setTextColor("inverted.text");
            title().setHoverTextColor("inverted.text", ButtonWidget::ReplaceColor);
            title().margins().setLeft("").setRight("");

            switch(type)
            {
            case AvailableGames:
                menu = new SingleplayerSessionMenuWidget(SingleplayerSessionMenuWidget::ShowAvailableGames);
                break;

            case IncompleteGames:
                menu = new SingleplayerSessionMenuWidget(SingleplayerSessionMenuWidget::ShowGamesWithMissingResources);
                break;

            case MultiplayerGames:
                menu = new MPSessionMenuWidget(MPSessionMenuWidget::DiscoverUsingMaster);
                break;

            case SavedGames:
                menu = new SavedSessionMenuWidget;
                break;
            }

            QObject::connect(menu,         SIGNAL(sessionSelected(de::ui::Item const *)),
                             &owner->self, SLOT  (select(de::ui::Item const *)));
            QObject::connect(menu,         SIGNAL(availabilityChanged()),
                             &owner->self, SLOT  (updateSubsetLayout()));

            menu->items().audienceForAddition() += this;

            setContent(menu);
            menu->margins().set("");
            menu->layout().setColumnPadding(owner->style().rules().rule("unit"));
            menu->rule().setInput(Rule::Width,
                                  owner->self.rule().width() -
                                  owner->self.margins().width());

            setColumns(3);

            // This will be shown if there are no games in the subset.
            // Note that this label is one of the menu's children, too,
            // in addition to the selectable sessions.
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

            menu->setColumns(cols);
        }

        ui::Data &items()
        {
            return menu->items();
        }

        String textForTitle(bool whenOpen) const
        {
            if(whenOpen) return titleText;
            return QString("%1 (%2)").arg(titleText).arg(menu->count());
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

    SequentialLayout superLayout;
    GameFilterWidget *filter;
    SubsetWidget *available;
    SubsetWidget *incomplete;
    SubsetWidget *multi;
    SubsetWidget *saved;
    QList<SubsetWidget *> subsets; // not owned
    bool doAction;

    Instance(Public *i)
        : Base(i)
        , superLayout(i->contentRule().left(), i->contentRule().top(), ui::Down)
        , doAction(false)
    {
        // Menu of available games.
        self.add(available = new SubsetWidget("available", SubsetWidget::AvailableGames,
                                              App_GameLoaded()? tr("Switch Game") :
                                                                tr("Available Games"), this));

        // Menu of incomplete games.
        self.add(incomplete = new SubsetWidget("incomplete", SubsetWidget::IncompleteGames,
                                               tr("Games with Missing Resources"), this));

        // Menu of multiplayer games.
        self.add(multi = new SubsetWidget("multi", SubsetWidget::MultiplayerGames,
                                          tr("Multiplayer Games"), this));

        // Menu of saved games.
        self.add(saved = new SubsetWidget("saved", SubsetWidget::SavedGames,
                                          tr("Saved Games"), this));

        // Keep all sets in a handy list.
        subsets << available << incomplete << multi << saved;

        self.add(filter = new GameFilterWidget);
        foreach(SubsetWidget *sub, subsets) sub->menu->setFilter(filter);

        superLayout.setOverrideWidth(self.rule().width() - self.margins().width());

        updateSubsetLayout();

        App::app().audienceForGameChange() += this;
    }

    ~Instance()
    {
        foreach(SubsetWidget *sub, subsets) sub->menu->setFilter(0);

        App::app().audienceForGameChange() -= this;
    }

    /**
     * Determines if a subset should be visible according to the current filter value.
     */
    bool isSubsetVisible(SubsetWidget const *sub) const
    {
        GameFilterWidget::Filter flt = filter->filter();
        if(sub == available || sub == incomplete || sub == saved)
        {
            return flt.testFlag(GameFilterWidget::Singleplayer);
        }
        if(sub == multi)
        {
            return flt.testFlag(GameFilterWidget::Multiplayer);
        }
        return false;
    }

    void updateSubsetVisibility()
    {
        foreach(SubsetWidget *sub, subsets)
        {
            bool shown = isSubsetVisible(sub);
            sub->show(shown);
            sub->title().show(shown);
        }
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
            order << available << multi << saved << incomplete;
        }
        else
        {
            order << multi << available << saved << incomplete;
        }

        updateSubsetVisibility();

        // Filter out the requested subsets.
        GameFilterWidget::Filter flt = filter->filter();
        if(!flt.testFlag(GameFilterWidget::Singleplayer))
        {
            order.removeOne(available);
            order.removeOne(incomplete);
            order.removeOne(saved);
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

    void updateLayoutForWidth(int width)
    {
        // If the view is too small, we'll want to reduce the number of items in the menu.
        int const maxWidth = style().rules().rule("gameselection.max.width").valuei();

        int const suitable = clamp(1, 4 * width / maxWidth, 3);
        foreach(SubsetWidget *s, subsets)
        {
            s->setColumns(suitable);
        }
    }
};

GameSelectionWidget::GameSelectionWidget(String const &name)
    : ScrollAreaWidget(name), d(new Instance(this))
{
    enableIndicatorDraw(true);
    setScrollBarColor("inverted.accent");

    // By default attach the filter above the widget.
    d->filter->rule()
            .setInput(Rule::Width,  rule().width())
            .setInput(Rule::Bottom, rule().top())
            .setInput(Rule::Left,   rule().left());

    // Default open/closed folds.
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

    connect(d->filter, SIGNAL(filterChanged()), this, SLOT(updateSubsetLayout()));
}

void GameSelectionWidget::setTitleColor(DotPath const &colorId,
                                        DotPath const &hoverColorId,
                                        ButtonWidget::HoverColorMode mode)
{
    foreach(Instance::SubsetWidget *s, d->subsets)
    {
        s->setTitleColor(colorId, hoverColorId, mode);
    }
}

void GameSelectionWidget::setTitleFont(DotPath const &fontId)
{
    foreach(Instance::SubsetWidget *s, d->subsets)
    {
        s->title().setFont(fontId);
    }
}

GameFilterWidget &GameSelectionWidget::filter()
{
    return *d->filter;
}

FoldPanelWidget *GameSelectionWidget::subsetFold(String const &name)
{
    return find(name)->maybeAs<FoldPanelWidget>();
}

void GameSelectionWidget::enableActionOnSelection(bool doAction)
{
    d->doAction = doAction;
}

Action *GameSelectionWidget::makeAction(ui::Item const &item) const
{
    // Find the session menu that owns this item.
    foreach(Instance::SubsetWidget *sub, d->subsets)
    {
        ui::DataPos const pos = sub->menu->items().find(item);
        if(pos != ui::Data::InvalidPos)
        {
            return sub->menu->makeAction(item);
        }
    }
    DENG2_ASSERT(!"GameSelectionWidget: Item does not belong in any subset");
    return 0;
}

void GameSelectionWidget::update()
{
    ScrollAreaWidget::update();

    // Adapt grid layout for the widget width.
    Rectanglei rect;
    if(hasChangedPlace(rect))
    {
        d->updateLayoutForWidth(rect.width());
    }
}

void GameSelectionWidget::operator >> (PersistentState &toState) const
{
    Record &st = toState.names();
    foreach(Instance::SubsetWidget *s, d->subsets)
    {
        // Save the fold open/closed state.
        st.set(name() + "." + s->name() + ".open", s->isOpen());
    }    
}

void GameSelectionWidget::operator << (PersistentState const &fromState)
{
    Record const &st = fromState.names();
    foreach(Instance::SubsetWidget *s, d->subsets)
    {
        // Restore the fold open/closed state.
        if(st[name() + "." + s->name() + ".open"].value().isTrue())
        {
            s->open(); // automatically shows the panel
        }
        else
        {
            s->close(0); // instantly
        }
    }

    // Ensure subsets that are supposed to be hidden stay that way.
    d->updateSubsetVisibility();
}

void GameSelectionWidget::updateSubsetLayout()
{
    d->updateSubsetLayout();
}

void GameSelectionWidget::select(ui::Item const *item)
{
    if(!item) return;

    emit gameSessionSelected(item);

    // Should we also perform the action?
    if(d->doAction)
    {
        AutoRef<Action> act = makeAction(*item);
        act->trigger();
    }
}
