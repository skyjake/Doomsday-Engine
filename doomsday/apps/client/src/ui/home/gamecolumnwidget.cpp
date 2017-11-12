/** @file gamecolumnwidget.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/dialogs/createprofiledialog.h"

#include <doomsday/DoomsdayApp>
#include <doomsday/Games>
#include <doomsday/GameProfiles>

#include <de/CallbackAction>
#include <de/ChildWidgetOrganizer>
#include <de/Config>
#include <de/DirectoryListDialog>
#include <de/Loop>
#include <de/MenuWidget>
#include <de/PersistentState>
#include <de/PopupMenuWidget>
#include <de/SignalAction>

using namespace de;

DENG_GUI_PIMPL(GameColumnWidget)
, DENG2_OBSERVES(Games, Readiness)
, DENG2_OBSERVES(Profiles, Addition)
, DENG2_OBSERVES(Variable, Change)
, DENG2_OBSERVES(ButtonWidget, StateChange)
, public ChildWidgetOrganizer::IWidgetFactory
{
    /**
     * Menu item for a game profile.
     */
    struct ProfileItem
            : public ui::Item
            , DENG2_OBSERVES(Deletable, Deletion) // profile deletion
    {
        GameColumnWidget::Impl *d;
        GameProfile *profile;

        ProfileItem(GameColumnWidget::Impl *d, GameProfile &gameProfile)
            : d(d)
            , profile(&gameProfile)
        {
            //setData(game().id());
            profile->audienceForDeletion += this;
        }

        Game const &game() const
        {
            DENG2_ASSERT(profile != nullptr);
            return profile->game();
        }

        //String gameId() const { return data().toString(); }

        void update() const { notifyChange(); }

        void objectWasDeleted(Deletable *obj)
        {
            DENG2_ASSERT(static_cast<GameProfile *>(obj) == profile);
            DENG2_UNUSED(obj);

            profile = nullptr;
            d->addOrRemoveSubheading();

            auto &items = d->menu->items();
            items.remove(items.find(*this)); // item deleted
        }

        DENG2_CAST_METHODS()
    };

    LoopCallback mainCall;
    String gameFamily;
    SaveListData const &savedItems;
    HomeMenuWidget *menu;
    ButtonWidget *newProfileButton;
    int restoredSelected = -1;
    bool gotSubheading = false;

    Impl(Public *i,
         String const &gameFamily,
         SaveListData const &savedItems)
        : Base(i)
        , gameFamily(gameFamily)
        , savedItems(savedItems)
    {
        ScrollAreaWidget &area = self().scrollArea();

        area.add(menu = new HomeMenuWidget);
        menu->organizer().setWidgetFactory(*this);
        menu->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Left,  area.contentRule().left())
                .setInput(Rule::Top,   self().header().rule().bottom());
        menu->margins().setBottom("");

        area.add(newProfileButton = new ButtonWidget);
        newProfileButton->audienceForStateChange() += this;
        newProfileButton->setText(tr("New Profile..."));
        newProfileButton->setStyleImage("create");
        newProfileButton->setOverrideImageSize(style().fonts().font("default").height().value() * 1.5f);
        newProfileButton->set(Background());
        newProfileButton->setSizePolicy(ui::Filled, ui::Expand);
        newProfileButton->setTextAlignment(ui::AlignRight);
        newProfileButton->setOpacity(actionOpacity());

        // Clicking the New Profile button creates a new profile. It gets added to the
        // menu via observers.
        newProfileButton->setActionFn([this] ()
        {
            auto *dlg = new CreateProfileDialog(this->gameFamily);
            dlg->setDeleteAfterDismissed(true);
            dlg->setAnchorAndOpeningDirection(newProfileButton->rule(), ui::Up);
            if (dlg->exec(root()))
            {
                // Adding the profile has the side effect that a widget is
                // created for it.
                auto *added = dlg->makeProfile();
                DoomsdayApp::gameProfiles().add(added);
            }
        });

        newProfileButton->rule()
                .setInput(Rule::Left,  area.contentRule().left())
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Top,   menu->rule().bottom());

        DoomsdayApp::games().audienceForReadiness() += this;
        Config::get("home.showUnplayableGames").audienceForChange() += this;
    }

    ui::Item *findProfileItem(GameProfile const &profile) const
    {
        for (dsize i = 0; i < menu->items().size(); ++i)
        {
            ui::Item &item = menu->items().at(i);
            if (!item.semantics().testFlag(ui::Item::Separator))
            {
                if (item.as<ProfileItem>().profile == &profile) return &item;
            }
        }
        return nullptr;
    }

    GamePanelButtonWidget &widgetForItem(ui::Item const &item) const
    {
        DENG2_ASSERT(menu->items().find(item) != ui::Data::InvalidPos);
        return menu->itemWidget<GamePanelButtonWidget>(item);
    }

    int userProfileCount() const
    {
        int count = 0;
        menu->items().forAll([this, &count] (ui::Item const &item)
        {
            if (!item.semantics().testFlag(ui::Item::Separator))
            {
                auto const *profile = item.as<ProfileItem>().profile;
                if (profile && profile->isUserCreated()) ++count;
            }
            return LoopContinue;
        });
        return count;
    }

    bool addItemForProfile(GameProfile &profile)
    {
        auto const &games = DoomsdayApp::games();
        if (games.contains(profile.gameId()))
        {
            if (profile.game().family() == gameFamily)
            {
                DENG2_ASSERT(!findProfileItem(profile));
                menu->items() << new ProfileItem(this, profile);
                addOrRemoveSubheading();
                return true;
            }
        }
        return false;
    }

    void profileAdded(Profiles::AbstractProfile &prof)
    {
        // This may be called from another thread.
        mainCall.enqueue([this, &prof] ()
        {
            if (addItemForProfile(prof.as<GameProfile>()))
            {
                sortItems();

                // Highlight the newly added item.
                auto const *newItem = findProfileItem(prof.as<GameProfile>());
                DENG2_ASSERT(newItem);
                menu->setSelectedIndex(menu->items().find(*newItem));
            }
        });
    }

    void addOrRemoveSubheading()
    {
        int const userCount = userProfileCount();

        if (userCount > 0 && !gotSubheading)
        {
            gotSubheading = true;
            menu->items() << new ui::Item(ui::Item::Separator, tr("Custom Profiles"));
        }
        else if (!userCount && gotSubheading)
        {
            for (dsize pos = 0; pos < menu->items().size(); ++pos)
            {
                if (menu->items().at(pos).semantics().testFlag(ui::Item::Separator))
                {
                    menu->items().remove(pos);
                    break;
                }
            }
            gotSubheading = false;
        }
    }

    /**
     * Populates the game items using the currently available game profiles.
     */
    void populateItems()
    {
        QSet<GameProfile *> profiles;
        foreach (GameProfile *prof,
                 DoomsdayApp::gameProfiles().profilesInFamily(gameFamily))
        {
            profiles.insert(prof);
        }
        QSet<GameProfile *> toAdd = profiles;

        // Update or remove profiles as needed.
        for (ui::DataPos i = 0; i < menu->items().size(); ++i)
        {
            ui::Item const &item = menu->items().at(i);
            if (item.semantics() & ui::Item::Separator)
            {
                // Skip the subheading.
                continue;
            }
            auto const &profItem = item.as<ProfileItem>();
            if (profiles.contains(profItem.profile))
            {
                // Already existing item.
                toAdd.remove(profItem.profile);
                profItem.update();
            }
            else
            {
                // Deleted profile.
                menu->items().remove(i--);
            }
        }

        // Add new items.
        foreach (GameProfile *newProf, toAdd)
        {
            addItemForProfile(*newProf);
        }

        addOrRemoveSubheading();
        sortItems();
    }

    enum Section { BuiltIn, Subheading, Custom };

    static Section itemSection(ui::Item const &item)
    {
        // The list is divided into three sections.
        if (item.semantics().testFlag(ui::Item::Separator)) return Subheading;
        return item.as<ProfileItem>().profile->isUserCreated()? Custom : BuiltIn;
    }

    void sortItems()
    {
        menu->items().sort([] (ui::Item const &a, ui::Item const &b)
        {
            Section const section1 = itemSection(a);
            Section const section2 = itemSection(b);

            if (section1 < section2)
            {
                return true;
            }
            if (section1 > section2)
            {
                return false;
            }

            GameProfile const &prof1 = *a.as<ProfileItem>().profile;
            GameProfile const &prof2 = *b.as<ProfileItem>().profile;

            if (section1 == Custom)
            {
                // Sorted alphabetically.
                return prof1.name().compareWithoutCase(prof2.name()) < 0;
            }

            // Sort built-in games by release date.
            int year = a.as<ProfileItem>().game().releaseDate().year() -
                       b.as<ProfileItem>().game().releaseDate().year();
            if (!year)
            {
                // Playable profiles first.
                if (prof1.isPlayable() && !prof2.isPlayable()) return true;
                if (!prof1.isPlayable() && prof2.isPlayable()) return false;

                // Finally, based on identifier.
                return prof1.gameId().compareWithoutCase(prof2.gameId()) < 0;
            }
            return year < 0;
        });
    }

    void updateItems()
    {
        menu->items().forAll([] (ui::Item const &item)
        {
            if (!item.semantics().testFlag(ui::Item::Separator))
            {
                item.as<ProfileItem>().update();
            }
            return LoopContinue;
        });
        menu->updateLayout();
    }

    void gameReadinessUpdated()
    {
        populateItems();

        // Restore earlier selection?
        if (restoredSelected >= 0)
        {
            menu->setSelectedIndex(restoredSelected);
            restoredSelected = -1;
        }

        // Observe further additions one by one.
        DoomsdayApp::gameProfiles().audienceForAddition() += this;
    }

    void variableValueChanged(Variable &, Value const &)
    {
        updateItems();
    }

//- ChildWidgetOrganizer::IWidgetFactory ------------------------------------------------

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *)
    {
        if (item.semantics().testFlag(ui::Item::Separator))
        {
            auto *heading = LabelWidget::newWithText(tr("Custom Profiles"));
            heading->setSizePolicy(ui::Filled, ui::Expand);
            heading->setFont("heading");
            //heading->setTextColor("accent");
            heading->setAlignment(ui::AlignLeft);
            heading->margins().setLeftRight("");
            heading->setOpacity(.75f);
            return heading;
        }

        auto const *profileItem = &item.as<ProfileItem>();
        auto *button = new GamePanelButtonWidget(*profileItem->profile, savedItems);

        // Right-clicking on game items shows additional functions.
        QObject::connect(button, &HomeItemWidget::openContextMenu, [this, button, profileItem] ()
        {
            auto *popup = new PopupMenuWidget;
            button->add(popup);
            popup->setDeleteAfterDismissed(true);
            popup->setAnchorAndOpeningDirection(button->label().rule(), ui::Down);

            bool const isUserProfile = profileItem->profile->isUserCreated();

            if (isUserProfile)
            {
                popup->items()
                    << new ui::ActionItem(tr("Edit..."), new CallbackAction([this, button, profileItem] ()
                    {
                        auto *dlg = CreateProfileDialog::editProfile(gameFamily, *profileItem->profile);
                        dlg->setAnchorAndOpeningDirection(button->label().rule(), ui::Up);
                        dlg->setAnchorX(button->rule().midX()); // keep centered in column
                        dlg->setDeleteAfterDismissed(true);
                        if (dlg->exec(root()))
                        {
                            dlg->applyTo(*profileItem->profile);
                            profileItem->update();
                        }
                    }));
            }
            else
            {
                popup->items() << new ui::ActionItem(tr("Packages..."), new SignalAction(button, SLOT(selectPackages())));
            }

            // Items suitable for all types of profiles.
            popup->items()
                << new ui::ActionItem(tr("Clear Packages"), new CallbackAction([this, button] ()
                {
                    button->clearPackages();
                }))
                << new ui::ActionItem(tr("Duplicate"), new CallbackAction([this, profileItem] ()
                {
                    GameProfile *dup = new GameProfile(*profileItem->profile);
                    dup->setUserCreated(true);

                    // Generate a unique name.
                    for (int attempt = 1; ; ++attempt)
                    {
                        String newName;
                        if (attempt > 1) newName = String("%1 (Copy %2)").arg(dup->name()).arg(attempt);
                                   else newName = String("%1 (Copy)").arg(dup->name());
                        if (!DoomsdayApp::gameProfiles().tryFind(newName))
                        {
                            dup->setName(newName);
                            DoomsdayApp::gameProfiles().add(dup);
                            break;
                        }
                    }
                }));

            if (isUserProfile)
            {
                auto *deleteSub = new ui::SubmenuItem(style().images().image("close.ring"),
                                                      tr("Delete"), ui::Left);
                deleteSub->items()
                    << new ui::Item(ui::Item::Separator, tr("Are you sure?"))
                    << new ui::ActionItem(tr("Delete Profile"),
                                          new CallbackAction([this, button, profileItem, popup] ()
                    {
                        popup->detachAnchor();
                        // Animate the widget to fade it away.
                        TimeSpan const SPAN = 0.2;
                        button->setOpacity(0, SPAN);
                        Loop::get().timer(SPAN, [profileItem] ()
                        {
                            delete profileItem->profile;
                        });
                    }))
                    << new ui::ActionItem(tr("Cancel"), new Action);

                popup->items()
                    << new ui::Item(ui::Item::Separator)
                    << deleteSub;
            }
            popup->open();
        });

        return button;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item)
    {
        if (item.semantics().testFlag(ui::Item::Separator)) return; // Ignore.

        auto &drawer = widget.as<GamePanelButtonWidget>();
        drawer.updateContent();

        if (!Config::get().getb("home.showUnplayableGames"))
        {
            drawer.show(item.as<ProfileItem>().game().isPlayableWithDefaultPackages());
        }
        else
        {
            drawer.show();
        }
    }

//- Actions -----------------------------------------------------------------------------

    float actionOpacity() const
    {
        return self().isHighlighted()? .4f : 0.f;
    }

    void buttonStateChanged(ButtonWidget &button, ButtonWidget::State state)
    {
        TimeSpan const SPAN = 0.25;
        switch (state)
        {
        case ButtonWidget::Up:
            button.setOpacity(actionOpacity(), SPAN);
            break;

        case ButtonWidget::Hover:
            button.setOpacity(.8f, SPAN);
            break;

        case ButtonWidget::Down:
            button.setOpacity(1);
            break;
        }
    }

    void showActions(bool show)
    {
        newProfileButton->setOpacity(show? actionOpacity() : 0, 0.6);
    }
};

GameColumnWidget::GameColumnWidget(String const &gameFamily,
                                   SaveListData const &savedItems)
    : ColumnWidget(gameFamily.isEmpty()? "other-column"
                                       : (gameFamily.toLower() + "-column"))
    , d(new Impl(this, gameFamily.toLower(), savedItems))
{
    scrollArea().setContentSize(maximumContentWidth(),
                                header().rule().height() +
                                rule("gap") +
                                d->menu->rule().height() +
                                d->newProfileButton->rule().height());

    header().title().setText(String(_E(s)_E(C) "%1\n" _E(.)_E(.)_E(w) "%2")
                             .arg( gameFamily == "DOOM"? "id Software" :
                                  !gameFamily.isEmpty()? "Raven Software" : "")
                             .arg(!gameFamily.isEmpty()? QString(gameFamily)
                                                       : tr("Other Games")));
    if (!gameFamily.isEmpty())
    {
        header().setLogoImage("logo.game." + gameFamily.toLower());
        header().setLogoBackground("home.background." + d->gameFamily);
        setBackgroundImage("home.background." + d->gameFamily);
    }
    else
    {
        setBackgroundImage("home.background.other");
    }

    header().menuButton().hide(); // no items for the menu atm

    /// @todo Get these description from the game family defs.
    {
    if (name() == "doom-column")
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
    else if (name() == "heretic-column")
    {
        header().info().setText("Raven Software released Heretic in 1994. It used "
                                "a modified version of id Software's DOOM engine. "
                                "The game featured such enhancements as inventory "
                                "management and the ability to look up and down. "
                                "Ambient sound effects were used to improve the "
                                "atmosphere of the game world.");
    }
    else if (name() == "hexen-column")
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

    //d->populateItems();
}

String GameColumnWidget::tabHeading() const
{
    if (d->gameFamily.isEmpty()) return tr("Other");
    return d->gameFamily.at(0).toUpper() + d->gameFamily.mid(1);
}

String GameColumnWidget::tabShortcut() const
{
    if (name() == "hexen-column") return QStringLiteral("x");
    return String();
}

String GameColumnWidget::configVariableName() const
{
    return "home.columns." + (!d->gameFamily.isEmpty()? d->gameFamily
                                                      : String("otherGames"));
}

void GameColumnWidget::setHighlighted(bool highlighted)
{
    ColumnWidget::setHighlighted(highlighted);

    if (highlighted)
    {
        d->menu->restorePreviousSelection();
    }
    else
    {
        root().setFocus(nullptr);
        d->menu->unselectAll();
    }
    d->showActions(highlighted);
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
