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
#include "clientapp.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/gameprofiles.h>

#include <de/callbackaction.h>
#include <de/childwidgetorganizer.h>
#include <de/config.h>
#include <de/directorylistdialog.h>
#include <de/filesystem.h>
#include <de/gridpopupwidget.h>
#include <de/loop.h>
#include <de/menuwidget.h>
#include <de/persistentstate.h>
#include <de/popupmenuwidget.h>
#include <de/variablechoicewidget.h>
#include <de/variabletogglewidget.h>

using namespace de;

const char *GameColumnWidget::SORT_GAME_ID("game");
const char *GameColumnWidget::SORT_MODS("mods");
const char *GameColumnWidget::SORT_RECENTLY_PLAYED("recent");
const char *GameColumnWidget::SORT_RELEASE_DATE("release");
const char *GameColumnWidget::SORT_TITLE("title");

static const char *VAR_SORT_BY                = "home.sortBy";
static const char *VAR_SORT_ASCENDING         = "home.sortAscending";
static const char *VAR_SORT_CUSTOM_SEPARATELY = "home.sortCustomSeparately";

DE_GUI_PIMPL(GameColumnWidget)
, DE_OBSERVES(Games, Readiness)
, DE_OBSERVES(Profiles, Addition)
, DE_OBSERVES(Variable, Change)
, DE_OBSERVES(ButtonWidget, StateChange)
, DE_OBSERVES(Profiles::AbstractProfile, Change)
, public ChildWidgetOrganizer::IWidgetFactory
{
    /**
     * Menu item for a game profile.
     */
    struct ProfileItem
            : public ui::Item
            , DE_OBSERVES(Deletable, Deletion) // profile deletion
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

        const Game &game() const
        {
            DE_ASSERT(profile != nullptr);
            return profile->game();
        }

        //String gameId() const { return data().toString(); }

        void update() const { notifyChange(); }

        void objectWasDeleted(Deletable *obj)
        {
            DE_ASSERT(static_cast<GameProfile *>(obj) == profile);
            DE_UNUSED(obj);

            profile = nullptr;
            d->addOrRemoveSubheading();

            auto &items = d->menu->items();
            items.remove(items.find(*this)); // item deleted
        }

        DE_CAST_METHODS()
    };

    Dispatch dispatch;
    String gameFamily;
    const SaveListData &savedItems;
    HomeMenuWidget *menu;
    ButtonWidget *newProfileButton;
    int restoredSelected = -1;
    bool gotSubheading = false;

    Impl(Public *i,
         const String &gameFamily,
         const SaveListData &savedItems)
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
        newProfileButton->setText("New Profile...");
        newProfileButton->setStyleImage("create");
        newProfileButton->setOverrideImageSize(style().fonts().font("default").height() * 1.5f);
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
                // created for it in the menu.
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
        Config::get(VAR_SORT_BY).audienceForChange() += this;
        Config::get(VAR_SORT_ASCENDING).audienceForChange() += this;
        Config::get(VAR_SORT_CUSTOM_SEPARATELY).audienceForChange() += this;
    }

    ui::Item *findProfileItem(const GameProfile &profile) const
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

    GamePanelButtonWidget &widgetForItem(const ui::Item &item) const
    {
        DE_ASSERT(menu->items().find(item) != ui::Data::InvalidPos);
        return menu->itemWidget<GamePanelButtonWidget>(item);
    }

    int userProfileCount() const
    {
        int count = 0;
        menu->items().forAll([&count] (const ui::Item &item)
        {
            if (!item.semantics().testFlag(ui::Item::Separator))
            {
                const auto *profile = item.as<ProfileItem>().profile;
                if (profile && profile->isUserCreated()) ++count;
            }
            return LoopContinue;
        });
        return count;
    }

    bool addItemForProfile(GameProfile &profile)
    {
        const auto &games = DoomsdayApp::games();
        if (games.contains(profile.gameId()))
        {
            if (profile.game().family() == gameFamily)
            {
                DE_ASSERT(!findProfileItem(profile));
                profile.upgradePackages();
                menu->items() << new ProfileItem(this, profile);
                addOrRemoveSubheading();
                profile.audienceForChange() += this;
                return true;
            }
        }
        return false;
    }

    void profileAdded(Profiles::AbstractProfile &prof) override
    {
        // This may be called from another thread.
        dispatch += [this, &prof] ()
        {
            if (addItemForProfile(prof.as<GameProfile>()))
            {
                sortItems();

                // Highlight the newly added item.
                const auto *newItem = findProfileItem(prof.as<GameProfile>());
                DE_ASSERT(newItem);
                menu->setSelectedIndex(menu->items().find(*newItem));
            }
        };
    }

    void profileChanged(Profiles::AbstractProfile &) override
    {
        Loop::timer(0.1, [this]() { sortItems(); });
    }

    bool isSubheadingVisibleWithSortOptions() const
    {
        return Config::get().getb(VAR_SORT_CUSTOM_SEPARATELY, true);
    }

    void addOrRemoveSubheading()
    {
        const int  userCount         = userProfileCount();
        const bool subheadingVisible = isSubheadingVisibleWithSortOptions() && userCount > 0;

        if (subheadingVisible && !gotSubheading)
        {
            gotSubheading = true;
            menu->items() << new ui::Item(ui::Item::Separator, "Custom Profiles");
        }
        else if (!subheadingVisible && gotSubheading)
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
        Set<GameProfile *> profiles;
        for (GameProfile *prof : DoomsdayApp::gameProfiles().profilesInFamily(gameFamily))
        {
            profiles.insert(prof);
        }
        Set<GameProfile *> toAdd = profiles;

        // Update or remove profiles as needed.
        for (ui::DataPos i = 0; i < menu->items().size(); ++i)
        {
            const ui::Item &item = menu->items().at(i);
            if (item.semantics() & ui::Item::Separator)
            {
                // Skip the subheading.
                continue;
            }
            const auto &profItem = item.as<ProfileItem>();
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
        for (GameProfile *newProf : toAdd)
        {
            addItemForProfile(*newProf);
        }

        addOrRemoveSubheading();
        sortItems();
    }

    enum Section { BuiltIn, Subheading, Custom };

    static Section itemSection(const ui::Item &item)
    {
        // The list is divided into three sections.
        if (item.semantics().testFlag(ui::Item::Separator)) return Subheading;
        return item.as<ProfileItem>().profile->isUserCreated()? Custom : BuiltIn;
    }

    void sortItems()
    {
        const String sortBy               = Config::get().gets(VAR_SORT_BY, SORT_RELEASE_DATE);
        const bool   sortAscending        = Config::get().getb(VAR_SORT_ASCENDING, true);
        const bool   sortCustomSeparately = Config::get().getb(VAR_SORT_CUSTOM_SEPARATELY, true);

        const auto *oldSelectedItem = menu->selectedItem();

        menu->items().sort([&sortBy, sortAscending, sortCustomSeparately](const ui::Item &a,
                                                                          const ui::Item &b) {
            const Section section1 = itemSection(a);
            const Section section2 = itemSection(b);

            if (sortCustomSeparately)
            {
                if (section1 < section2)
                {
                    return true;
                }
                if (section1 > section2)
                {
                    return false;
                }
            }

            const GameProfile &prof1 = *a.as<ProfileItem>().profile;
            const GameProfile &prof2 = *b.as<ProfileItem>().profile;

            // Sort built-in games by release date.
            int cmp = 0;

            if (sortBy == SORT_RELEASE_DATE)
            {
                cmp = a.as<ProfileItem>().game().releaseDate().year() -
                      b.as<ProfileItem>().game().releaseDate().year();
            }
            else if (sortBy == SORT_GAME_ID)
            {
                cmp = a.as<ProfileItem>().game().id().compare(b.as<ProfileItem>().game().id());
            }
            else if (sortBy == SORT_TITLE)
            {
                cmp = prof1.name().compareWithoutCase(prof2.name());
            }
            else if (sortBy == SORT_MODS)
            {
                cmp = de::cmp(prof1.packages().size(), prof2.packages().size());
            }
            else if (sortBy == SORT_RECENTLY_PLAYED)
            {
                if (prof1.lastPlayedAt().isValid() && prof2.lastPlayedAt().isValid())
                {
                    cmp = -de::cmp(prof1.lastPlayedAt(), prof2.lastPlayedAt());
                }
                else if (prof1.lastPlayedAt().isValid())
                {
                    cmp = -1;
                }
                else if (prof2.lastPlayedAt().isValid())
                {
                    cmp = +1;
                }
            }

            if (cmp == 0)
            {
                // Secondary sort order.

                // Playable profiles first.
                if (prof1.isPlayable() && !prof2.isPlayable()) return sortAscending;
                if (!prof1.isPlayable() && prof2.isPlayable()) return !sortAscending;

                cmp = prof1.name().compareWithoutCase(prof2.name());
                if (cmp == 0)
                {
                    // Finally, based on identifier.
                    cmp = prof1.gameId().compareWithoutCase(prof2.gameId());
                }
            }
            if (!sortAscending)
            {
                cmp = -cmp;
            }
            return cmp < 0;
        });

        if (oldSelectedItem)
        {
            menu->setSelectedIndex(menu->items().find(*oldSelectedItem));
        }
    }

    void updateItems()
    {
        menu->items().forAll([] (const ui::Item &item)
        {
            if (!item.semantics().testFlag(ui::Item::Separator))
            {
                item.as<ProfileItem>().update();
            }
            return LoopContinue;
        });
        menu->updateLayout();
    }

    void gameReadinessUpdated() override
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

    void variableValueChanged(Variable &var, const Value &) override
    {
        if (var.name().beginsWith("sort"))
        {
            addOrRemoveSubheading();
            sortItems();
        }
        else
        {
            updateItems();
        }
    }

//- ChildWidgetOrganizer::IWidgetFactory ------------------------------------------------

    GuiWidget *makeItemWidget(const ui::Item &item, const GuiWidget *) override
    {
        if (item.semantics().testFlag(ui::Item::Separator))
        {
            auto *heading = LabelWidget::newWithText("Custom Profiles");
            heading->setSizePolicy(ui::Filled, ui::Expand);
            heading->setFont("heading");
            //heading->setTextColor("accent");
            heading->setAlignment(ui::AlignLeft);
            heading->margins().setLeftRight("");
            heading->setOpacity(.75f);
            return heading;
        }

        const auto *profileItem = &item.as<ProfileItem>();
        auto *button = new GamePanelButtonWidget(*profileItem->profile, savedItems);

        // Right-clicking on game items shows additional functions.
        button->audienceForContextMenu() += [this, button, profileItem]()
        {
            auto *popup = new PopupMenuWidget;
            button->add(popup);
            popup->setDeleteAfterDismissed(true);
            popup->setAnchorAndOpeningDirection(button->label().rule(), ui::Down);

            const bool isUserProfile = profileItem->profile->isUserCreated();

            if (isUserProfile)
            {
                popup->items()
                    << new ui::ActionItem("Edit...", [this, button, profileItem]()
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
                    });
            }

            // Items suitable for all types of profiles.
            popup->items()
                << new ui::ActionItem("Select Mods...",
                                      [button]() { button->selectPackages(); })
                << new ui::ActionItem("Clear Mods", [button]() { button->clearPackages(); })
                << new ui::ActionItem(
                       "Duplicate", new CallbackAction([profileItem]() {
                           GameProfile *dup = new GameProfile(*profileItem->profile);
                           dup->setUserCreated(true);
                           dup->createSaveLocation();

                           // Generate a unique name.
                           for (int attempt = 1;; ++attempt)
                           {
                               String newName;
                               if (attempt > 1)
                                   newName = Stringf("%s (Copy %d)", dup->name().c_str(), attempt);
                               else
                                   newName = Stringf("%s (Copy)", dup->name().c_str());
                               if (!DoomsdayApp::gameProfiles().tryFind(newName))
                               {
                                   dup->setName(newName);
                                   DoomsdayApp::gameProfiles().add(dup);
                                   break;
                               }
                           }
                       }));

            if (const auto *loc = FS::tryLocate<const Folder>(profileItem->profile->savePath()))
            {
                popup->items() << new ui::Item(ui::Item::Separator)
                               << new ui::ActionItem("Show Save Folder", [loc]() {
                                      ClientApp::app().showLocalFile(
                                          loc->correspondingNativePath());
                                  });
            }

            if (isUserProfile && !profileItem->profile->saveLocationId())
            {
                // Old profiles don't have their own save locations. Offer to create one here.
                popup->items()
                    << new ui::ActionItem(
                           "Create New Save Folder", new CallbackAction([this, profileItem]() {
                               profileItem->profile->createSaveLocation();
                               // Inform the user.
                               auto *msg = new MessageDialog;
                               msg->setDeleteAfterDismissed(true);
                               msg->title().setText("Save Folder Created");
                               msg->message().setText(
                                   "Save files of the profile " _E(b) +
                                   profileItem->profile->name() + _E(.) " will be written to " +
                                   FS::locate<const Folder>(profileItem->profile->savePath())
                                       .description() + ".");
                               msg->buttons() << new DialogButtonItem(DialogWidget::Accept |
                                                                      DialogWidget::Default);
                               msg->exec(root());
                           }));
            }

            if (isUserProfile)
            {
                auto *deleteSub = new ui::SubmenuItem(style().images().image("close.ring"),
                                                      "Delete", ui::Left);
                deleteSub->items()
                    << new ui::Item(ui::Item::Separator, "Are you sure?")
                    << new ui::ActionItem(
                           "Delete Profile",
                           new CallbackAction([this, button, profileItem, popup]() {
                               if (profileItem->profile->saveLocationId())
                               {
                                   const Folder *saveFolder =
                                       FS::tryLocate<const Folder>(profileItem->profile->savePath());

                                   if (saveFolder && !profileItem->profile->isSaveLocationEmpty())
                                   {
                                       // What to do with the savegames?
                                       auto *question = new MessageDialog;
                                       question->setDeleteAfterDismissed(true);
                                       question->title().setText("Delete Saved Games?");
                                       question->title().setStyleImage("alert");
                                       question->message().setText(
                                           "The profile " _E(b) + profileItem->profile->name() +
                                           _E(.) " that is being deleted has saved games. "
                                                 "Do you wish to delete the save files as well?");
                                       const NativePath savePath =
                                           saveFolder->correspondingNativePath();
                                       question->buttons()
                                           << new DialogButtonItem(DialogWidget::Accept,
                                                                   "Delete All")
                                           << new DialogButtonItem(DialogWidget::Reject |
                                                                       DialogWidget::Default,
                                                                   "Cancel")
                                           << new DialogButtonItem(
                                                  DialogWidget::Action,
                                                  "Show Folder",
                                                  [savePath]() {
                                                      ClientApp::app().showLocalFile(savePath);
                                                  });
                                       if (!question->exec(root()))
                                       {
                                           // Cancelled.
                                           return;
                                       }
                                   }
                                   if (saveFolder)
                                   {
                                       profileItem->profile->destroySaveLocation();
                                   }
                               }

                               // Animate the widget to fade it away.
                               const TimeSpan SPAN = 0.2;
                               button->setOpacity(0, SPAN);
                               popup->detachAnchor();
                               popup->close();
                               Loop::get().timer(SPAN,
                                                 [profileItem]() { delete profileItem->profile; });
                           }))
                    << new ui::ActionItem("Cancel", new Action);

                popup->items()
                    << new ui::Item(ui::Item::Separator)
                    << deleteSub;
            }
            popup->open();
        };

        return button;
    }

    void updateItemWidget(GuiWidget &widget, const ui::Item &item) override
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

    void buttonStateChanged(ButtonWidget &button, ButtonWidget::State state) override
    {
        const TimeSpan SPAN = 0.25;
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

GameColumnWidget::GameColumnWidget(const String &gameFamily,
                                   const SaveListData &savedItems)
    : ColumnWidget(gameFamily.isEmpty()? "other-column"
                                       : (gameFamily.lower() + "-column"))
    , d(new Impl(this, gameFamily.lower(), savedItems))
{
    scrollArea().setContentSize(maximumContentWidth(),
                                header().rule().height() +
                                rule("gap") +
                                d->menu->rule().height() +
                                d->newProfileButton->rule().height());

    header().title().setText(Stringf(_E(s)_E(C) "%s\n" _E(.)_E(.)_E(w) "%s",
                              gameFamily == "DOOM"? "id Software" : gameFamily ? "Raven Software" : "",
                              gameFamily ? gameFamily.c_str() : "Other Games"));
    if (gameFamily)
    {
        header().setLogoImage("logo.game." + gameFamily.lower());
        header().setLogoBackground("home.background." + d->gameFamily);
        setBackgroundImage("home.background." + d->gameFamily);
    }
    else
    {
        setBackgroundImage("home.background.other");
    }

    // View options for the game columns.
    {
        header().menuButton().setPopup(
            [](const PopupButtonWidget &) -> PopupWidget * {
                auto *pop = new GridPopupWidget;
                auto *sortBy =
                    new VariableChoiceWidget(Config::get(VAR_SORT_BY), VariableChoiceWidget::Text);
                sortBy->items() << new ChoiceItem("Recently played", SORT_RECENTLY_PLAYED)
                                << new ChoiceItem("Release date", SORT_RELEASE_DATE)
                                << new ChoiceItem("Title", SORT_TITLE)
                                << new ChoiceItem("Mods", SORT_MODS)
                                << new ChoiceItem("Game ID", SORT_GAME_ID);
                sortBy->updateFromVariable();
                *pop << LabelWidget::newWithText("Show:")
                     << new VariableToggleWidget("Descriptions",
                                                 Config::get("home.showColumnDescription"))
                     << Const(0)
                     << new VariableToggleWidget("Unplayable Games",
                                                 Config::get("home.showUnplayableGames"))
                     << LabelWidget::newWithText("Sort By:") << sortBy << Const(0)
                     << new VariableToggleWidget("Ascending", Config::get(VAR_SORT_ASCENDING))
                     << Const(0)
                     << new VariableToggleWidget("Separate Custom",
                                                 Config::get(VAR_SORT_CUSTOM_SEPARATELY));
                pop->commit();
                return pop;
            },
            ui::Down);
    }

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
}

String GameColumnWidget::tabHeading() const
{
    if (d->gameFamily.isEmpty()) return "Other";
    return d->gameFamily.upperFirstChar();
}

int GameColumnWidget::tabShortcut() const
{
    if (name() == "doom-column")    return 'd';
    if (name() == "heretic-column") return 'h';
    if (name() == "hexen-column")   return 'x';
    return 'o';
}

String GameColumnWidget::configVariableName() const
{
    return "home.columns." + (!d->gameFamily.isEmpty() ? d->gameFamily
                                                      : DE_STR("otherGames"));
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

void GameColumnWidget::operator>>(PersistentState &toState) const
{
    Record &rec = toState.objectNamespace();
    rec.set(name().concatenateMember("selected"), d->menu->selectedIndex());
}

void GameColumnWidget::operator<<(const PersistentState &fromState)
{
    const Record &rec = fromState.objectNamespace();
    d->restoredSelected = rec.geti(name().concatenateMember("selected"), -1);
}
