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
#include "ui/home/gamepanelbuttonwidget.h"
#include "ui/widgets/homemenuwidget.h"

#include <doomsday/DoomsdayApp>
#include <doomsday/Games>
#include <doomsday/GameProfiles>

#include <de/ChildWidgetOrganizer>
#include <de/MenuWidget>
#include <de/PersistentState>
#include <de/Loop>
#include <de/App>

using namespace de;

DENG_GUI_PIMPL(GameColumnWidget)
, DENG2_OBSERVES(Games, Readiness)
, DENG2_OBSERVES(Profiles, Addition)
, DENG2_OBSERVES(Variable, Change)
, public ChildWidgetOrganizer::IWidgetFactory
{
    // Item for a particular loadable game.
    struct MenuItem
            : public ui::Item
            , DENG2_OBSERVES(Deletable, Deletion)
    {
        GameColumnWidget::Instance *d;
        GameProfile const *profile;

        MenuItem(GameColumnWidget::Instance *d, GameProfile const &gameProfile)
            : d(d)
            , profile(&gameProfile)
        {
            setData(game().id());
            profile->audienceForDeletion += this;
        }

        ~MenuItem()
        {
            if(profile) profile->audienceForDeletion -= this;
        }

        Game const &game() const
        {
            DENG2_ASSERT(profile != nullptr);
            return DoomsdayApp::games()[profile->game()];
        }

        String gameId() const { return data().toString(); }

        void update() const { notifyChange(); }

        void objectWasDeleted(Deletable *obj)
        {
            DENG2_ASSERT(static_cast<GameProfile *>(obj) == profile);

            profile = nullptr;

            auto &items = d->menu->items();
            items.remove(items.find(*this)); // item deleted
        }

        DENG2_AS_IS_METHODS()
    };

    LoopCallback mainCall;
    String gameFamily;
    SavedSessionListData const &savedItems;
    HomeMenuWidget *menu;
    int restoredSelected = -1;

    Instance(Public *i,
             String const &gameFamily,
             SavedSessionListData const &savedItems)
        : Base(i)
        , gameFamily(gameFamily)
        , savedItems(savedItems)
    {
        ScrollAreaWidget &area = self.scrollArea();

        area.add(menu = new HomeMenuWidget);
        menu->organizer().setWidgetFactory(*this);

        menu->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Left,  area.contentRule().left())
                .setInput(Rule::Top,   self.header().rule().bottom());

        DoomsdayApp::games().audienceForReadiness() += this;
        DoomsdayApp::gameProfiles().audienceForAddition() += this;
        App::config("home.showUnplayableGames").audienceForChange() += this;
    }

    ~Instance()
    {
        DoomsdayApp::games().audienceForReadiness() -= this;
        DoomsdayApp::gameProfiles().audienceForAddition() -= this;
        App::config("home.showUnplayableGames").audienceForChange() -= this;
    }

    ui::Item const *findItem(String const &id) const
    {
        auto const pos = menu->items().findData(id);
        if(pos == ui::Data::InvalidPos) return nullptr;
        return &menu->items().at(pos);
    }

    GamePanelButtonWidget &widgetForItem(ui::Item const &item) const
    {
        DENG2_ASSERT(menu->items().find(item) != ui::Data::InvalidPos);
        return menu->itemWidget<GamePanelButtonWidget>(item);
    }

    void addItemForProfile(GameProfile const &profile)
    {
        if(DoomsdayApp::games()[profile.game()].family() == gameFamily)
        {
            menu->items() << new MenuItem(this, profile);
        }
    }

    void profileAdded(Profiles::AbstractProfile &prof)
    {
        // This may be called from another thread.
        mainCall.enqueue([this, &prof] ()
        {
            addItemForProfile(prof.as<GameProfile>());
            sortItems();
        });
    }

    /**
     * Populates the game items using the currently available game profiles.
     */
    void populateItems()
    {
        menu->items().clear();
        DoomsdayApp::gameProfiles().forAll([this] (GameProfile &profile)
        {
            addItemForProfile(profile);
            return LoopContinue;
        });
        sortItems();
    }

    void sortItems()
    {
        menu->items().sort([] (ui::Item const &a, ui::Item const &b)
        {
            // Sorting by game release date.
            return a.as<MenuItem>().game().releaseDate().year() <
                   b.as<MenuItem>().game().releaseDate().year();
        });
    }

    void updateItems()
    {
        menu->items().forAll([] (ui::Item const &item)
        {
            item.as<MenuItem>().update();
            return LoopContinue;
        });
    }

    void gameReadinessUpdated()
    {
        updateItems();

        // Restore earlier selection?
        if(restoredSelected >= 0)
        {
            menu->setSelectedIndex(restoredSelected);
            restoredSelected = -1;
        }
    }

    void variableValueChanged(Variable &, Value const &)
    {
        updateItems();
    }

//- ChildWidgetOrganizer::IWidgetFactory ------------------------------------------------

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *)
    {
        auto *button = new GamePanelButtonWidget(item.as<MenuItem>().game(), savedItems);
        return button;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item)
    {
        auto &drawer = widget.as<GamePanelButtonWidget>();
        drawer.updateContent();

        if(!App::config().getb("home.showUnplayableGames"))
        {
            drawer.show(item.as<MenuItem>().game().isPlayable());
        }
        else
        {
            drawer.show();
        }
    }
};

GameColumnWidget::GameColumnWidget(String const &gameFamily,
                                   SavedSessionListData const &savedItems)
    : ColumnWidget(gameFamily.isEmpty()? "other-column"
                                       : (gameFamily.toLower() + "-column"))
    , d(new Instance(this, gameFamily.toLower(), savedItems))
{
    scrollArea().setContentSize(maximumContentWidth(),
                                header().rule().height() +
                                rule("gap") +
                                d->menu->rule().height());

    header().title().setText(String(_E(s) "%1\n" _E(.)_E(w) "%2")
                             .arg( gameFamily == "DOOM"? "id Software" :
                                  !gameFamily.isEmpty()? "Raven Software" : "")
                             .arg(!gameFamily.isEmpty()? QString(gameFamily)
                                                       : tr("Other Games")));
    if(!gameFamily.isEmpty())
    {
        header().setLogoImage("logo.game." + gameFamily.toLower());
        header().setLogoBackground("home.background." + d->gameFamily);
        setBackgroundImage("home.background." + d->gameFamily);
    }

    /// @todo Get these description from the game family defs.
    {
    if(name() == "doom-column")
    {
        header().info().setText("id Software released DOOM for MS-DOS in 1993. "
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
        header().info().setText("Raven Software released Heretic in 1994. It used "
                                "a modified version of id Software's DOOM engine. "
                                "The game featured such enhancements as inventory "
                                "management and the ability to look up and down. "
                                "Ambient sound effects were used to improve the "
                                "atmosphere of the game world.");
    }
    else if(name() == "hexen-column")
    {
        header().info().setText("Raven Software released Hexen in 1996. The "
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
        header().info().setText("Thanks to its excellent modding support, DOOM has "
                                "been used as a basis for many games and community "
                                "projects.");
    }
    }

    d->populateItems();
}

String GameColumnWidget::tabHeading() const
{
    if(d->gameFamily.isEmpty()) return tr("Other");
    return d->gameFamily.at(0).toUpper() + d->gameFamily.mid(1);
}

String GameColumnWidget::configVariableName() const
{
    return "home.columns." + (!d->gameFamily.isEmpty()? d->gameFamily
                                                      : String("otherGames"));
}

void GameColumnWidget::setHighlighted(bool highlighted)
{
    ColumnWidget::setHighlighted(highlighted);

    if(!highlighted)
    {
        d->menu->unselectAll();
    }
}

void GameColumnWidget::operator >> (PersistentState &toState) const
{
    Record &rec = toState.objectNamespace();
    rec.set(name().concatenateMember("selected"), d->menu->selectedIndex());
}

void GameColumnWidget::operator << (PersistentState const &fromState)
{
    Record const &rec = fromState.objectNamespace();
    d->restoredSelected = rec.geti(name().concatenateMember("selected"), -1);
}
