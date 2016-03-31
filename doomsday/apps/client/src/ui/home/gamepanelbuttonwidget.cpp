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
#include <de/StyleProceduralImage>

using namespace de;

DENG_GUI_PIMPL(GamePanelButtonWidget)
, public ChildWidgetOrganizer::IFilter
{
    GameProfile &gameProfile;
    SavedSessionListData const &savedItems;
    SaveListWidget *saves;
    PackagesButtonWidget *packagesButton;
    ButtonWidget *playButton;
    ButtonWidget *deleteSaveButton;
    res::LumpCatalog catalog;

    Instance(Public *i, GameProfile &profile, SavedSessionListData const &savedItems)
        : Base(i)
        , gameProfile(profile)
        , savedItems(savedItems)
    {
        self.icon().setBehavior(ContentClipping);
        self.icon().setImageFit(ui::FitToSize | ui::CoverArea | ui::OriginalAspectRatio);

        packagesButton = new PackagesButtonWidget;
        packagesButton->setDialogTitle(profile.name());
        self.addButton(packagesButton);

        QObject::connect(packagesButton,
                         &PackagesButtonWidget::packageSelectionChanged,
                         [this] (QStringList ids)
        {
            StringList pkgs;
            for(auto const &i : ids) pkgs << i;
            gameProfile.setPackages(pkgs);
            if(catalog.setPackages(gameProfile.allRequiredPackages()))
            {
                updateGameTitleImage();
            }
        });

        playButton = new ButtonWidget;
        playButton->useInfoStyle();
        playButton->setImage(new StyleProceduralImage("play", self));
        playButton->setImageColor(style().colors().colorf("inverted.text"));
        playButton->setOverrideImageSize(style().fonts().font("default").height().value());
        playButton->setActionFn([this] () { playButtonPressed(); });
        self.addButton(playButton);

        // List of saved games.
        saves = new SaveListWidget(self);
        saves->rule().setInput(Rule::Width, self.rule().width());
        saves->margins().setZero().setLeft(self.icon().rule().width());
        saves->organizer().setFilter(*this);
        saves->setItems(savedItems);

        deleteSaveButton = new ButtonWidget;
        deleteSaveButton->setImage(new StyleProceduralImage("close.ring", self));
        deleteSaveButton->setOverrideImageSize(style().fonts().font("default").height().value());
        deleteSaveButton->setSizePolicy(ui::Expand, ui::Expand);
        deleteSaveButton->set(Background());
        deleteSaveButton->hide();
        deleteSaveButton->setActionFn([this] () { deleteButtonPressed(); });
        self.panel().add(deleteSaveButton);

        self.panel().setContent(saves);
        self.panel().open();
    }

    Game const &game() const
    {
        return DoomsdayApp::games()[gameProfile.game()];
    }

    void playButtonPressed()
    {
        BusyMode_FreezeGameForBusyMode();
        //ClientWindow::main().taskBar().close();
        // TODO: Emit a signal that hides the Home and closes the taskbar.

        // Switch the game.
        DoomsdayApp::app().changeGame(game(), DD_ActivateGameWorker);

        if(saves->selectedPos() != ui::Data::InvalidPos)
        {
            // Load a saved game.
            auto const &saveItem = savedItems.at(saves->selectedPos());
            Con_Execute(CMDS_DDAY, ("loadgame " + saveItem.name() + " confirm").toLatin1(),
                        false, false);
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
        if(!game().isPlayable())
        {
            // Use a generic logo, some files are missing.
            QImage img(64, 64, QImage::Format_ARGB32);
            img.fill(Qt::white);
            self.icon().setImage(img);
            return;
        }

        try
        {
            Block const playPal  = catalog.read("PLAYPAL");
            Block const title    = catalog.read("TITLE");
            Block const titlePic = catalog.read("TITLEPIC");

            IdTech1Image img(title.isEmpty()? titlePic : title, playPal);

            // Apply VGA aspect correction while downscaling 50%.
            self.icon().setImage(img.toQImage().scaled(img.width()/2,
                                                       img.height()/2 * 1.2f,
                                                       Qt::IgnoreAspectRatio,
                                                       Qt::SmoothTransformation));
        }
        catch(Error const &er)
        {
            LOG_RES_WARNING("Failed to load title picture for game profile \"%s\": %s")
                    << gameProfile.name()
                    << er.asText();
        }
    }

//- ChildWidgetOrganizer::IFilter ---------------------------------------------

    bool isItemAccepted(ChildWidgetOrganizer const &,
                        ui::Data const &data, ui::Data::Pos pos) const
    {
        // User-created profiles currently have no saves associated with them.
        if(gameProfile.isUserCreated()) return false;

        // Only saved sessions for this game are to be included.
        auto const &item = data.at(pos).as<SavedSessionListData::SaveItem>();
        return item.gameId() == gameProfile.game();
    }
};

GamePanelButtonWidget::GamePanelButtonWidget(GameProfile &game, SavedSessionListData const &savedItems)
    : d(new Instance(this, game, savedItems))
{
    connect(d->saves, SIGNAL(selectionChanged(de::ui::DataPos)), this, SLOT(saveSelected(de::ui::DataPos)));
    connect(d->saves, SIGNAL(doubleClicked(de::ui::DataPos)), this, SLOT(saveDoubleClicked(de::ui::DataPos)));
    connect(this, SIGNAL(doubleClicked()), this, SLOT(play()));
}

void GamePanelButtonWidget::setSelected(bool selected)
{
    PanelButtonWidget::setSelected(selected);

    d->playButton->enable(selected);

    if(!selected)
    {
        unselectSave();
    }

    updateContent();
}

void GamePanelButtonWidget::updateContent()
{
    enable(d->game().isPlayable());

    String meta = !d->gameProfile.isUserCreated()? String::number(d->game().releaseDate().year())
                                                 : d->game().title();

    if(isSelected())
    {
        if(d->saves->selectedPos() != ui::Data::InvalidPos)
        {
            meta = tr("Restore saved game");
        }
        /*else if(d->saves->childCount() > 0)
        {
            meta = tr("%1 saved game%2")
                    .arg(d->saves->childCount())
                    .arg(d->saves->childCount() != 1? "s" : "");
        }*/
        else if(!d->gameProfile.isUserCreated())
        {
            meta = tr("Start new session");
        }
    }

    label().setText(String(_E(b) "%1\n" _E(l) "%2")
                    .arg(d->gameProfile.name())
                    .arg(meta));

    d->packagesButton->setPackages(d->gameProfile.packages());
    if(d->catalog.setPackages(d->gameProfile.allRequiredPackages()))
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
    if(savePos != ui::Data::InvalidPos)
    {
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
