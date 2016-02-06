/** @file gamecolumnwidget.cpp
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

#include "ui/home/gamecolumnwidget.h"
#include "ui/home/headerwidget.h"
#include "ui/home/gamedrawerbutton.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>

#include <de/ChildWidgetOrganizer>
#include <de/MenuWidget>

using namespace de;

DENG_GUI_PIMPL(GameColumnWidget)
, DENG2_OBSERVES(Games, Readiness)
, public ChildWidgetOrganizer::IWidgetFactory
{
    // Item for a particular loadable game.
    struct MenuItem : public ui::Item
    {
        Game const &game;

        MenuItem(Game const &game) : game(game) { setData(game.id()); }
        String gameId() const { return data().toString(); }
        void update() const { notifyChange(); }

        DENG2_AS_IS_METHODS()
    };

    String gameFamily;
    SavedSessionListData const &savedItems;
    HeaderWidget *header;
    MenuWidget *menu;
    Image bgImage;

    Instance(Public *i,
             String const &gameFamily,
             SavedSessionListData const &savedItems)
        : Base(i)
        , gameFamily(gameFamily)
        , savedItems(savedItems)
    {
        ScrollAreaWidget &area = self.scrollArea();

        area.add(header = new HeaderWidget);

        header->rule()
                .setInput(Rule::Left,  area.contentRule().left())
                .setInput(Rule::Top,   area.contentRule().top())
                .setInput(Rule::Width, area.contentRule().width());

        area.add(menu = new MenuWidget);
        menu->enableScrolling(false);
        menu->enablePageKeys(false);
        menu->setGridSize(1, ui::Filled, 0, ui::Expand);
        menu->organizer().setWidgetFactory(*this);
        menu->layout().setRowPadding(style().rules().rule("gap")*2);

        menu->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Left,  area.contentRule().left())
                .setInput(Rule::Top,   header->rule().bottom() +
                                       style().rules().rule("gap")*2);

        DoomsdayApp::games().audienceForReadiness() += this;
    }

    ~Instance()
    {
        DoomsdayApp::games().audienceForReadiness() -= this;
    }

    ui::Item const *findItem(String const &id) const
    {
        auto const pos = menu->items().findData(id);
        if(pos == ui::Data::InvalidPos) return nullptr;
        return &menu->items().at(pos);
    }

    GameDrawerButton &widgetForItem(ui::Item const &item) const
    {
        DENG2_ASSERT(menu->items().find(item) != ui::Data::InvalidPos);
        return menu->itemWidget<GameDrawerButton>(item);
    }

    void populateItems()
    {
        Games const &games = DoomsdayApp::games();

        // Add games not already in the list.
        for(int i = 0; i < games.count(); ++i)
        {
            Game const &game = games.byIndex(i);
            if(game.family() == gameFamily)
            {
                if(auto const *item = findItem(game.id()))
                {
                    item->as<MenuItem>().update();
                }
                else
                {
                    menu->items() << new MenuItem(game);
                }
            }
        }

        // Remove items no longer available.
        for(ui::DataPos i = menu->items().size() - 1; i < menu->items().size(); --i)
        {
            MenuItem const &item = menu->items().at(i).as<MenuItem>();
            if(!games.contains(item.gameId()))
            {
                // Time to remove this one.
                menu->items().remove(i);
            }
        }

        menu->items().sort([] (ui::Item const &a, ui::Item const &b) {
            return a.as<MenuItem>().game.releaseDate().year() <
                   b.as<MenuItem>().game.releaseDate().year();
        });
    }

    void gameReadinessUpdated()
    {
        populateItems();
    }

//- ChildWidgetOrganizer::IWidgetFactory --------------------------------------

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *)
    {
        auto *button = new GameDrawerButton(item.as<MenuItem>().game, savedItems);
        QObject::connect(button, SIGNAL(clicked()), thisPublic, SLOT(itemClicked()));
        return button;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item)
    {
        MenuItem const &menuItem = item.as<MenuItem>();
        auto &drawer = widget.as<GameDrawerButton>();
        drawer.updateContent();
    }
};

GameColumnWidget::GameColumnWidget(String const &gameFamily,
                                   SavedSessionListData const &savedItems)
    : ColumnWidget(gameFamily.toLower() + "-column")
    , d(new Instance(this, gameFamily.toLower(), savedItems))
{
    scrollArea().setContentSize(maximumContentWidth(),
                                d->header->rule().height() +
                                style().rules().rule("gap") +
                                d->menu->rule().height());

    d->header->title().setText(String(_E(s) "%1\n" _E(.)_E(w) "%2")
                               .arg( gameFamily == "DOOM"? "id Software" :
                                    !gameFamily.isEmpty()? "Raven Software" : "")
                               .arg(!gameFamily.isEmpty()? QString(gameFamily) : tr("Other Games")));
    if(!gameFamily.isEmpty())
    {
        d->header->setLogoImage("logo.game." + gameFamily.toLower());
        d->header->setLogoBackground("home.background." + d->gameFamily);
        d->bgImage = style().images().image("home.background." + d->gameFamily);
        setBackgroundImage(d->bgImage);
    }

    /// @todo Get these description from the game family defs.
    {
    if(name() == "doom-column")
    {
        d->header->info().setText("id Software released DOOM for MS-DOS in 1993. "
                                  "It soon became a massive hit and is regarded as "
                                  "the game that popularized the first-person shooter "
                                  "genre. Since then the franchise has been continued "
                                  "in several sequels, starting with DOOM II: Hell on "
                                  "Earth in 1994. DOOM and many of its follow-ups "
                                  "have been ported to numerous other platforms, and "
                                  "to this day remains a favorite among gamers.");
    }
    else if(name() == "heretic-column")
    {
        d->header->info().setText("Raven Software released Heretic in 1994. It used "
                                  "a modified version of id Software's DOOM engine. "
                                  "The game featured such enhancements as inventory "
                                  "management and the ability to look up and down. "
                                  "Ambient sound effects were used to improve the "
                                  "atmosphere of the game world.");
    }
    else if(name() == "hexen-column")
    {
        d->header->info().setText("Raven Software released Hexen in 1996. The "
                                  "company had continued making heavy modifications "
                                  "to the DOOM engine, and Hexen introduced such "
                                  "sophisticated features as a scripting language "
                                  "for game events. The maps were well-designed and "
                                  "interconnected with each other, resulting in a "
                                  "more intriguing game world and more complex "
                                  "puzzles to solve.");
    }
    else
    {
        d->header->info().setText("Thanks to its excellent modding support, DOOM has "
                                  "been used as a basis for many games and community "
                                  "projects.");
    }
    }
}

void GameColumnWidget::setHighlighted(bool highlighted)
{
    ColumnWidget::setHighlighted(highlighted);

    /*if(highlighted)
    {
        setBackgroundImage(d->bgImage);
    }
    else
    {
        setBackgroundImage(Image::solidColor(Image::Color(0, 0, 0, 255),
                                             Image::Size(4, 4)));
    }*/

    //d->header->setOpacity(highlighted? 1 : .7, .5);
    //d->menu->setOpacity  (highlighted? 1 : .7, .5);
}

void GameColumnWidget::itemClicked()
{
    auto *clickedButton = dynamic_cast<GameDrawerButton *>(sender());

    // Other buttons should be deselected.
    for(auto *w : d->menu->childWidgets())
    {
        if(auto *button = w->maybeAs<GameDrawerButton>())
        {
            button->setSelected(button == clickedButton);
            button->updateContent();
        }
    }
}
