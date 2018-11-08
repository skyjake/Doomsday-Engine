/** @file gamepanelbuttonwidget.cpp  Panel button for games.
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

#include "ui/home/gamepanelbuttonwidget.h"
#include "ui/widgets/homemenuwidget.h"
#include "ui/home/savelistwidget.h"
#include "ui/home/gamecolumnwidget.h"
#include "ui/savelistdata.h"
#include "ui/dialogs/packagesdialog.h"
#include "ui/widgets/packagesbuttonwidget.h"
#include "resource/idtech1image.h"
#include "dd_main.h"

#include <doomsday/console/exec.h>
#include <doomsday/DoomsdayApp>
#include <doomsday/Games>
#include <doomsday/LumpCatalog>
#include <doomsday/LumpDirectory>
#include <doomsday/res/Bundles>

#include <de/App>
#include <de/Async>
#include <de/CallbackAction>
#include <de/ChildWidgetOrganizer>
#include <de/Config>
#include <de/FileSystem>
#include <de/Loop>
#include <de/PopupMenuWidget>
#include <de/ui/FilteredData>

using namespace de;

DENG_GUI_PIMPL(GamePanelButtonWidget)
, DENG2_OBSERVES(Profiles::AbstractProfile, Change)
, DENG2_OBSERVES(res::Bundles, Identify)
, DENG2_OBSERVES(Variable, Change)
, public AsyncScope
{
    GameProfile &gameProfile;
    ui::FilteredDataT<SaveListData::SaveItem> savedItems;
    SaveListWidget *saves;
    PackagesButtonWidget *packagesButton;
    ButtonWidget *playButton;
    ButtonWidget *deleteSaveButton;
    LabelWidget *problemIcon;
    LabelWidget *packagesCounter;
    res::LumpCatalog catalog;

    Impl(Public *i, GameProfile &profile, SaveListData const &allSavedItems)
        : Base(i)
        , gameProfile(profile)
        , savedItems(allSavedItems)
    {
        // Only show the savegames relevant for this game.
        savedItems.setFilter([this] (ui::Item const &it)
        {
            // Only saved sessions for this game are to be included.
            auto const &item = it.as<SaveListData::SaveItem>();
            if (item.gameId() != gameProfile.gameId())
            {
                return false;
            }

            if (!gameProfile.isPlayable()) return false;

            // The file must be in the right save folder.
            if (item.savePath().fileNamePath().compareWithoutCase(gameProfile.savePath()))
            {
                return false;
            }

            StringList const savePacks = item.loadedPackages();

            // Fallback for older saves without package metadata.
            if (savePacks.isEmpty())
            {
                // Show under the built-in profile.
                return !gameProfile.isUserCreated();
            }

            // Check if the packages used in the savegame are in conflict with the
            // profile's packages.
            if (!gameProfile.isCompatibleWithPackages(savePacks))
            {
                return false;
            }

            // Yep, seems fine.
            return true;
        });

        packagesButton = new PackagesButtonWidget;
        packagesButton->setGameProfile(gameProfile);
        packagesButton->setDialogTitle(tr("Mods for %1").arg(profile.name()));
        packagesButton->setSetupCallback([this] (PackagesDialog &dialog)
        {
            // Add a button for starting the game.
            dialog.buttons()
                    << new DialogButtonItem(DialogWidget::Action,
                                            style().images().image("play"),
                                            tr("Play"),
                                            new CallbackAction([this, &dialog] () {
                                                dialog.accept();
                                                playButtonPressed();
                                            }));
        });
        self().addButton(packagesButton);

        QObject::connect(packagesButton,
                         &PackagesButtonWidget::packageSelectionChanged,
                         [this] (QStringList ids)
        {
            gameProfile.setPackages(toStringList(ids));
            self().updateContent();
        });

        playButton = new ButtonWidget;
        playButton->useInfoStyle();
        playButton->setStyleImage("play", "default");
        playButton->setImageColor(style().colors().colorf("inverted.text"));
        playButton->setActionFn([this] () { playButtonPressed(); });
        self().addButton(playButton);

        // List of saved games.
        saves = new SaveListWidget(self());
        saves->rule().setInput(Rule::Width, self().rule().width());
        saves->margins().setZero().setLeft(self().icon().rule().width());
        saves->setItems(savedItems);

        deleteSaveButton = new ButtonWidget;
        deleteSaveButton->setStyleImage("close.ring", "default");
        deleteSaveButton->setSizePolicy(ui::Expand, ui::Expand);
        deleteSaveButton->set(Background());
        deleteSaveButton->hide();
        deleteSaveButton->setActionFn([this] () { deleteButtonPressed(); });
        self().panel().add(deleteSaveButton);

        problemIcon = new LabelWidget;
        problemIcon->setStyleImage("alert", "default");
        problemIcon->setImageColor(style().colors().colorf("accent"));
        problemIcon->rule().setRect(self().icon().rule());
        problemIcon->hide();
        self().icon().add(problemIcon);

        // Package count indicator (non-interactive).
        packagesCounter = new LabelWidget;
        packagesCounter->setOpacity(.5f);
        packagesCounter->setSizePolicy(ui::Expand, ui::Expand);
        packagesCounter->set(GuiWidget::Background());
        packagesCounter->setStyleImage("package.icon", "small");
        packagesCounter->setFont("small");
        packagesCounter->margins().setLeft("");
        packagesCounter->setTextAlignment(ui::AlignLeft);
        packagesCounter->rule()
                .setInput(Rule::Right, self().label().rule().right())
                .setMidAnchorY(self().label().rule().midY());
        packagesCounter->hide();
        self().label().add(packagesCounter);
        self().label().setMinimumHeight(style().fonts().font("default").lineSpacing() * 3 +
                                      self().label().margins().height());

        self().panel().setContent(saves);
        self().panel().open();

        DoomsdayApp::bundles().audienceForIdentify() += this;
        Config::get("home.sortBy").audienceForChange() += this;
    }

    Game const &game() const
    {
        return gameProfile.game();
    }

    void updatePackagesIndicator()
    {
        int const  count = gameProfile.packages().size();
        bool const shown = !isMissingPackages() && count > 0 && !self().isSelected();

        packagesCounter->setText(String::number(count));
        packagesCounter->show(shown);

        if (shown)
        {
            self().setLabelMinimumRightMargin(packagesCounter->rule().width());
        }
        else
        {
            self().setLabelMinimumRightMargin(Const(0));
        }
    }

    void playButtonPressed()
    {
        if (playButton->isEnabled())
        {
            BusyMode_FreezeGameForBusyMode();
            //ClientWindow::main().taskBar().close();
            // TODO: Emit a signal that hides the Home and closes the taskbar.

            gameProfile.setLastPlayedAt();

            // Switch the game.
            DoomsdayApp::app().changeGame(gameProfile, DD_ActivateGameWorker);

            if (saves->selectedPos() != ui::Data::InvalidPos)
            {
                // Load a saved game.
                auto const &saveItem = savedItems.at(saves->selectedPos());
                Con_Execute(CMDS_DDAY, ("loadgame " + saveItem.name() + " confirm").toLatin1(),
                            false, false);
            }
        }
    }

    void deleteButtonPressed()
    {
        DENG2_ASSERT(saves->selectedPos() != ui::Data::InvalidPos);

        // Popup to make sure.
        PopupMenuWidget *pop = new PopupMenuWidget;
        pop->setDeleteAfterDismissed(true);
        pop->setAnchorAndOpeningDirection(deleteSaveButton->rule(), ui::Down);
        pop->items()
                << new ui::Item(ui::Item::Separator, tr("Are you sure?"))
                << new ui::ActionItem(tr("Delete Savegame"),
                                      new CallbackAction([this, pop] ()
                {
                    pop->detachAnchor();
                    // Delete the savegame file; the UI will be automatically updated.
                    String const path = savedItems.at(saves->selectedPos()).savePath();
                    self().unselectSave();
                    App::rootFolder().destroyFile(path);
                    FS::get().refreshAsync();
                }))
                << new ui::ActionItem(tr("Cancel"), new Action /* nop */);
        self().add(pop);
        pop->open();
    }

    void updateGameTitleImage()
    {
        *this += async([this]() { return IdTech1Image::makeGameLogo(game(), catalog); },
                       [this](const Image &gameLogo) { self().icon().setImage(gameLogo); });
    }

    void profileChanged(Profiles::AbstractProfile &) override
    {
        self().updateContent();
    }

    void variableValueChanged(Variable &, Value const &) override
    {
        self().updateContent();
    }

    void dataBundlesIdentified() override
    {
        Loop::get().mainCall([this] ()
        {
            catalog.clear();
            self().updateContent();
        });
    }

    bool isMissingPackages() const
    {
        return !gameProfile.isPlayable() && game().isPlayableWithDefaultPackages();
    }

    String defaultSubtitle() const
    {
        if (gameProfile.isUserCreated())
        {
            return game().title();
        }
        if (Config::get("home.sortBy") != GameColumnWidget::SORT_RECENTLY_PLAYED)
        {
            return String::number(game().releaseDate().year());
        }
        return {};
    }
};

GamePanelButtonWidget::GamePanelButtonWidget(GameProfile &game, SaveListData const &savedItems)
    : d(new Impl(this, game, savedItems))
{
    connect(d->saves, SIGNAL(selectionChanged(de::ui::DataPos)), this, SLOT(saveSelected(de::ui::DataPos)));
    connect(d->saves, SIGNAL(doubleClicked(de::ui::DataPos)), this, SLOT(saveDoubleClicked(de::ui::DataPos)));
    connect(this, SIGNAL(doubleClicked()), this, SLOT(play()));

    game.audienceForChange() += d;
}

void GamePanelButtonWidget::setSelected(bool selected)
{
    if ((isSelected() && !selected) || (!isSelected() && selected))
    {
        PanelButtonWidget::setSelected(selected);

        d->playButton->enable(selected);

        if (!selected)
        {
            unselectSave();
        }

        updateContent();
    }
}

void GamePanelButtonWidget::updateContent()
{
    bool const isPlayable     = d->gameProfile.isPlayable();
    bool const isGamePlayable = d->game().isPlayableWithDefaultPackages();

    playButton().enable(isPlayable);
    playButton().show(isPlayable);
    updateButtonLayout();
    d->problemIcon->show(!isGamePlayable);
    d->updatePackagesIndicator();

    const String sortBy = Config::get("home.sortBy");

    String meta;
    if (sortBy == GameColumnWidget::SORT_RECENTLY_PLAYED)
    {
        meta = d->gameProfile.lastPlayedAt().isValid()
                   ? "Last played " + d->gameProfile.lastPlayedAt().asText(Time::FriendlyFormat)
                   : "";
    }
    else if (sortBy == GameColumnWidget::SORT_GAME_ID)
    {
        meta = d->game().id();
    }
    if (meta.isEmpty())
    {
        meta = d->defaultSubtitle();
    }

    if (isSelected())
    {
        if (!isGamePlayable)
        {
            meta = _E(D) + tr("Missing data files") + _E(.);
        }
        else if (d->saves->selectedPos() != ui::Data::InvalidPos)
        {
            meta = tr("Restore saved game");
        }
        else if (!d->gameProfile.isUserCreated())
        {
            meta = tr("Start new session");
        }
    }

    if (d->isMissingPackages())
    {
        meta = _E(D) "Missing mods" _E(.);
        d->packagesButton->setOverrideLabel("Fix...");
        setKeepButtonsVisible(true);
    }
    else
    {
        d->packagesButton->setOverrideLabel("");
        setKeepButtonsVisible(false);
    }

    label().setText(String(_E(b) "%1\n" _E(l) "%2")
                    .arg(d->gameProfile.name())
                    .arg(meta));

    d->packagesButton->setPackages(d->gameProfile.packages());
    d->updatePackagesIndicator();
    if (d->catalog.setPackages(d->gameProfile.allRequiredPackages()))
    {
        d->updateGameTitleImage();
    }
    d->savedItems.refilter();
}

void GamePanelButtonWidget::unselectSave()
{
    d->saves->clearSelection();
}

ButtonWidget &GamePanelButtonWidget::playButton()
{
    return *d->playButton;
}

bool GamePanelButtonWidget::handleEvent(Event const &event)
{
    if (hasFocus() && event.isKey())
    {
        auto const &key = event.as<KeyEvent>();
        if (event.isKeyDown() &&
            (key.ddKey() == DDKEY_ENTER || key.ddKey() == DDKEY_RETURN || key.ddKey() == ' '))
        {
            if (playButton().isEnabled())
            {
                root().setFocus(&playButton());
            }
            else
            {
                root().setFocus(d->packagesButton);
            }
        }
    }

    return PanelButtonWidget::handleEvent(event);
}

void GamePanelButtonWidget::play()
{
    d->playButtonPressed();
}

void GamePanelButtonWidget::selectPackages()
{
    d->packagesButton->trigger();
}

void GamePanelButtonWidget::clearPackages()
{
    d->gameProfile.setPackages(StringList());
    updateContent();
}

void GamePanelButtonWidget::saveSelected(de::ui::DataPos savePos)
{
    if (savePos != ui::Data::InvalidPos)
    {
        // Ensure that this game is selected.
        DENG2_ASSERT(parentMenu() != nullptr);
        parentMenu()->setSelectedIndex(parentMenu()->findItem(*this));

        // Position the save deletion button.
        GuiWidget &widget = d->saves->itemWidget<GuiWidget>(d->savedItems.at(savePos));
        d->deleteSaveButton->rule()
                .setMidAnchorY(widget.rule().midY())
                .setInput(Rule::Right, widget.rule().left());
        d->deleteSaveButton->show();
    }
    else
    {
        d->deleteSaveButton->hide();
    }

    updateContent();
}

void GamePanelButtonWidget::saveDoubleClicked(de::ui::DataPos savePos)
{
    d->saves->setSelectedPos(savePos);
    play();
}
