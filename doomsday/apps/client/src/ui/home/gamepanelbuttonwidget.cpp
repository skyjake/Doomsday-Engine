/** @file gamepanelbuttonwidget.cpp  Panel button for games.
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

#include "ui/home/gamepanelbuttonwidget.h"
#include "ui/widgets/homemenuwidget.h"
#include "ui/home/savelistwidget.h"
#include "ui/savedsessionlistdata.h"
#include "ui/dialogs/packagesdialog.h"
#include "ui/widgets/packagesbuttonwidget.h"
#include "resource/idtech1image.h"
#include "dd_main.h"

#include <doomsday/console/exec.h>
#include <doomsday/DoomsdayApp>
#include <doomsday/Games>
#include <doomsday/LumpCatalog>
#include <doomsday/LumpDirectory>

#include <de/App>
#include <de/CallbackAction>
#include <de/ChildWidgetOrganizer>
#include <de/PopupMenuWidget>
#include <de/ui/FilteredData>

using namespace de;

DENG_GUI_PIMPL(GamePanelButtonWidget)
{
    GameProfile &gameProfile;
    ui::FilteredDataT<SavedSessionListData::SaveItem> savedItems;
    SaveListWidget *saves;
    PackagesButtonWidget *packagesButton;
    ButtonWidget *playButton;
    ButtonWidget *deleteSaveButton;
    LabelWidget *problemIcon;
    LabelWidget *packagesCounter;
    res::LumpCatalog catalog;

    Impl(Public *i, GameProfile &profile, SavedSessionListData const &allSavedItems)
        : Base(i)
        , gameProfile(profile)
        , savedItems(allSavedItems)
    {
        // Only show the savegames relevant for this game.
        savedItems.setFilter([this] (ui::Item const &it)
        {
            // Only saved sessions for this game are to be included.
            auto const &item = it.as<SavedSessionListData::SaveItem>();
            if (item.gameId() != gameProfile.game())
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
        packagesButton->setDialogTitle(profile.name());
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
        self.addButton(packagesButton);

        QObject::connect(packagesButton,
                         &PackagesButtonWidget::packageSelectionChanged,
                         [this] (QStringList ids)
        {
            StringList pkgs;
            for (auto const &i : ids) pkgs << i;
            gameProfile.setPackages(pkgs);
            updatePackagesIndicator();
            if (catalog.setPackages(gameProfile.allRequiredPackages()))
            {
                updateGameTitleImage();
            }
        });

        playButton = new ButtonWidget;
        playButton->useInfoStyle();
        playButton->setStyleImage("play", "default");
        playButton->setImageColor(style().colors().colorf("inverted.text"));
        playButton->setActionFn([this] () { playButtonPressed(); });
        self.addButton(playButton);

        // List of saved games.
        saves = new SaveListWidget(self);
        saves->rule().setInput(Rule::Width, self.rule().width());
        saves->margins().setZero().setLeft(self.icon().rule().width());
        saves->setItems(savedItems);

        deleteSaveButton = new ButtonWidget;
        deleteSaveButton->setStyleImage("close.ring", "default");
        deleteSaveButton->setSizePolicy(ui::Expand, ui::Expand);
        deleteSaveButton->set(Background());
        deleteSaveButton->hide();
        deleteSaveButton->setActionFn([this] () { deleteButtonPressed(); });
        self.panel().add(deleteSaveButton);

        problemIcon = new LabelWidget;
        problemIcon->setStyleImage("alert", "default");
        problemIcon->setImageColor(style().colors().colorf("accent"));
        problemIcon->rule().setRect(self.icon().rule());
        problemIcon->hide();
        self.icon().add(problemIcon);

        // Package count indicator (non-interactive).
        packagesCounter = new LabelWidget;
        packagesCounter->setOpacity(.5f);
        packagesCounter->setSizePolicy(ui::Expand, ui::Expand);
        packagesCounter->set(GuiWidget::Background());
        packagesCounter->setStyleImage("package", "small");
        packagesCounter->setFont("small");
        packagesCounter->margins().setLeft("");
        packagesCounter->setTextAlignment(ui::AlignLeft);
        packagesCounter->rule()
                .setInput(Rule::Right, self.label().rule().right())
                .setMidAnchorY(self.label().rule().midY());
        packagesCounter->hide();
        self.label().add(packagesCounter);

        self.panel().setContent(saves);
        self.panel().open();
    }

    Game const &game() const
    {
        return DoomsdayApp::games()[gameProfile.game()];
    }

    void updatePackagesIndicator()
    {
        int const  count = gameProfile.packages().size();
        bool const shown = count > 0 && !self.isSelected();

        packagesCounter->setText(String::number(count));
        packagesCounter->show(shown);

        if (shown)
        {
            self.setLabelMinimumRightMargin(packagesCounter->rule().width());
        }
        else
        {
            self.setLabelMinimumRightMargin(Const(0));
        }
    }

    void playButtonPressed()
    {
        if (playButton->isEnabled())
        {
            BusyMode_FreezeGameForBusyMode();
            //ClientWindow::main().taskBar().close();
            // TODO: Emit a signal that hides the Home and closes the taskbar.

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
                                      new CallbackAction([this] ()
                {
                    // Delete the savegame file; the UI will be automatically updated.
                    String const path = savedItems.at(saves->selectedPos()).savePath();
                    self.unselectSave();
                    App::rootFolder().removeFile(path);
                    App::fileSystem().refresh();
                }))
                << new ui::ActionItem(tr("Cancel"), new Action /* nop */);
        self.add(pop);
        pop->open();
    }

    void updateGameTitleImage()
    {
        self.icon().setImage(self.makeGameLogo(game(), catalog));
    }
};

GamePanelButtonWidget::GamePanelButtonWidget(GameProfile &game, SavedSessionListData const &savedItems)
    : d(new Impl(this, game, savedItems))
{
    connect(d->saves, SIGNAL(selectionChanged(de::ui::DataPos)), this, SLOT(saveSelected(de::ui::DataPos)));
    connect(d->saves, SIGNAL(doubleClicked(de::ui::DataPos)), this, SLOT(saveDoubleClicked(de::ui::DataPos)));
    connect(this, SIGNAL(doubleClicked()), this, SLOT(play()));
}

void GamePanelButtonWidget::setSelected(bool selected)
{
    PanelButtonWidget::setSelected(selected);

    d->playButton->enable(selected);

    if (!selected)
    {
        unselectSave();
    }

    updateContent();
}

void GamePanelButtonWidget::updateContent()
{
    bool const isPlayable = d->game().isPlayable();

    playButton().enable(isPlayable);
    d->problemIcon->show(!isPlayable);
    d->updatePackagesIndicator();

    String meta = !d->gameProfile.isUserCreated()? String::number(d->game().releaseDate().year())
                                                 : d->game().title();

    if (isSelected())
    {
        if (!d->game().isPlayable())
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

    label().setText(String(_E(b) "%1\n" _E(l) "%2")
                    .arg(d->gameProfile.name())
                    .arg(meta));

    d->packagesButton->setPackages(d->gameProfile.packages());
    d->updatePackagesIndicator();
    if (d->catalog.setPackages(d->gameProfile.allRequiredPackages()))
    {
        d->updateGameTitleImage();
    }
}

void GamePanelButtonWidget::unselectSave()
{
    d->saves->clearSelection();
}

ButtonWidget &GamePanelButtonWidget::playButton()
{
    return *d->playButton;
}

void GamePanelButtonWidget::play()
{
    d->playButtonPressed();
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
