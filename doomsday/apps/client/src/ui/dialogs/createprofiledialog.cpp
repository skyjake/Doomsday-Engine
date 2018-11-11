/** @file createprofiledialog.cpp
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

#include "ui/dialogs/createprofiledialog.h"
#include "ui/widgets/packagesbuttonwidget.h"
#include "ui/widgets/packageswidget.h"
#include "ui/dialogs/datafilesettingsdialog.h"

#include <doomsday/DoomsdayApp>
#include <doomsday/Games>
#include <doomsday/GameProfiles>
#include <doomsday/DataBundle>

#include <de/AuxButtonWidget>
#include <de/CallbackAction>
#include <de/ChoiceWidget>
#include <de/GridLayout>
#include <de/DialogContentStylist>
#include <de/PackageLoader>

using namespace de;

DENG_GUI_PIMPL(CreateProfileDialog)
{
    ChoiceWidget *gameChoice;
    PackagesButtonWidget *packages;
    ChoiceWidget *autoStartMap;
    ChoiceWidget *autoStartSkill;
    AuxButtonWidget *customDataFileName;
    ui::ListData customDataFileActions;
    SafeWidgetPtr<PackagesWidget> customPicker;
    DialogContentStylist stylist;
    bool editing = false;
    String oldName;
    std::unique_ptr<GameProfile> tempProfile; // Used only internally.

    Impl(Public *i) : Base(i) {}

    void checkValidProfileName()
    {
        bool valid = false;

        String const entry = self().profileName();
        if (!entry.isEmpty())
        {
            if (editing && oldName == entry)
            {
                valid = true;
            }
            else
            {
                // Must be a new, unique name.
                valid = DoomsdayApp::gameProfiles().forAll([&entry] (const GameProfile &prof) {
                    if (!entry.compareWithoutCase(prof.name())) {
                        return LoopAbort;
                    }
                    return LoopContinue;
                }) == LoopContinue;
            }
        }

        // A game must be selected, too.
        if (!gameChoice->isValidSelection())
        {
            valid = false;
        }
        if (gameChoice->isValidSelection() &&
            gameChoice->selectedItem().data().toString().isEmpty())
        {
            valid = false;
        }
        self().buttonWidget(Id1)->enable(valid);
    }

    void gameChanged()
    {
        if (gameChoice->isValidSelection())
        {
            tempProfile->setGame(gameChoice->selectedItem().data().toString());
            // Used with the PackagesButtonWidget.
            updateMapList();
        }
    }

    void updateMapList()
    {
        auto &mapItems = autoStartMap->items();

        String oldChoice;
        if (!mapItems.isEmpty())
        {
            oldChoice = autoStartMap->selectedItem().data().toString();
        }

        mapItems.clear();
        mapItems << new ChoiceItem(tr("Title screen"), "");

        // Find out all the required and selected packages.
        StringList packageIds;
        if (gameChoice->isValidSelection())
        {
            packageIds += tempProfile->allRequiredPackages();
        }

        // Create menu items for the Start Map choice.
        for (int i = packageIds.size() - 1; i >= 0; --i)
        {
            String const pkgId = packageIds.at(i);
            if (File const *pkgFile = PackageLoader::get().select(pkgId))
            {
                DataBundle const *bundle = maybeAs<DataBundle>(pkgFile->target());
                if (!bundle || !bundle->lumpDirectory())
                {
                    continue;
                }

                auto const maps = bundle->lumpDirectory()->findMapLumpNames();
                if (!maps.isEmpty())
                {
                    mapItems << new ui::Item(ui::Item::Separator);

                    String const wadName = Package::metadata(*pkgFile).gets(Package::VAR_TITLE);
                    foreach (String mapId, maps)
                    {
                        // Only show each map identifier once; only the last lump can
                        // be loaded.
                        if (mapItems.findData(mapId) == ui::Data::InvalidPos)
                        {
                            mapItems << new ChoiceItem(String("%1  " _E(s)_E(C) "%2").arg(mapId).arg(wadName),
                                                       mapId);
                        }
                    }
                }
            }
        }

        auto const pos = mapItems.findData(oldChoice);
        autoStartMap->setSelected(pos != ui::Data::InvalidPos ? pos : 0);
    }

    void updateDataFile()
    {
        if (tempProfile->customDataFile().isEmpty())
        {
            customDataFileName->setText(_E(l) "Default game data");
        }
        else
        {
            if (const auto *pkgFile = PackageLoader::get().select(tempProfile->customDataFile()))
            {
                const auto path = pkgFile->correspondingNativePath();
                customDataFileName->setText(path.pretty());
            }
            else
            {
                customDataFileName->setText(_E(b) "Not found");
            }
        }
        updateMapList();
    }
};

CreateProfileDialog::CreateProfileDialog(String const &gameFamily)
    : InputDialog("create-profile")
    , d(new Impl(this))
{
    title()  .setText(tr("New Profile"));
    message().setText(tr("Enter a name for the new game profile."));

    auto *form = new GuiWidget;
    d->stylist.setContainer(*form);
    area().add(form);

    // Populate games list.
    form->add(d->gameChoice = new ChoiceWidget);
    DoomsdayApp::games().forAll([this, &gameFamily] (Game &game)
    {
        if (game.family() == gameFamily)
        {
            const char *labelColor = (!game.isPlayable() ? _E(F) : "");
            d->gameChoice->items() << new ChoiceItem(labelColor + game.title(), game.id());
        }
        return LoopContinue;
    });
    d->gameChoice->items().sort();

    if (d->gameChoice->items().isEmpty())
    {
        d->gameChoice->items() << new ChoiceItem(tr("No playable games"), "");
    }

    // Custom data file selection.
    {
        form->add(d->customDataFileName = new AuxButtonWidget);
        d->customDataFileName->setMaximumTextWidth(
            rule().right() - d->customDataFileName->rule().left() -
                    d->customDataFileName->margins().width());
        d->customDataFileName->setTextLineAlignment(ui::AlignLeft);
        d->customDataFileName->auxiliary().setText("Reset");
        d->customDataFileActions
                << new ui::ActionItem("Select", new CallbackAction([this]() {
            if (d->customPicker)
            {
                d->tempProfile->setCustomDataFile(d->customPicker->actionPackage());
                d->customPicker->ancestorOfType<MessageDialog>()->accept();
                d->updateDataFile();
            }
        }));
        d->customDataFileName->setActionFn([this]() {
            auto *dlg = new MessageDialog;
            dlg->buttons() << new DialogButtonItem(Reject, "Cancel")
                           << new DialogButtonItem(Action | Id1, style().images().image("gear"),
                                                   "Data Files", new CallbackAction([dlg]() {
                auto *dfsDlg = new DataFileSettingsDialog;
                dfsDlg->setAnchorAndOpeningDirection(dlg->buttonWidget(Id1)->rule(), ui::Up);
                dfsDlg->setDeleteAfterDismissed(true);
                dlg->add(dfsDlg);
                dfsDlg->open();
            }));
            dlg->setDeleteAfterDismissed(true);
            dlg->title().setText("Game Data File");
            dlg->message().setText(
                "Select the main data file for this profile. When using a manually chosen main "
                "data file, the default data files of the game will not be automatically loaded.");
            dlg->area().enableIndicatorDraw(true);
            d->customPicker.reset(new PackagesWidget);
            dlg->area().add(d->customPicker);
            d->customPicker->setHiddenTags({"hidden", "core"});
            d->customPicker->setAllowPackageInfoActions(false);
            d->customPicker->setActionItems(d->customDataFileActions);
            dlg->setAnchorAndOpeningDirection(d->customDataFileName->rule(), ui::Right);
            dlg->updateLayout();
            root().addOnTop(dlg);
            dlg->open(Modal);
        });
        d->customDataFileName->auxiliary().setActionFn([this]() {
            d->tempProfile->setCustomDataFile("");
            d->updateDataFile();
        });
    }

    // Packages selection.
    form->add(d->packages = new PackagesButtonWidget);
    d->packages->setNoneLabel(tr("None"));
    d->tempProfile.reset(new GameProfile);
    d->packages->setGameProfile(*d->tempProfile);

    // Auto start map.
    form->add(d->autoStartMap = new ChoiceWidget);
    d->autoStartMap->popup().menu().enableIndicatorDraw(true);
    d->autoStartMap->items() << new ChoiceItem(tr("Title screen"), "");

    // Auto start skill.
    form->add(d->autoStartSkill = new ChoiceWidget);
    d->autoStartSkill->items()
            << new ChoiceItem(tr("Novice"),    1)
            << new ChoiceItem(tr("Easy"),      2)
            << new ChoiceItem(tr("Normal"),    3)
            << new ChoiceItem(tr("Hard"),      4)
            << new ChoiceItem(tr("Nightmare"), 5);
    d->autoStartSkill->disable();
    d->autoStartSkill->setSelected(2);

    GridLayout layout(form->rule().left(), form->rule().top() + rule("dialog.gap"));
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *LabelWidget::newWithText(tr("Game:"), form)
           << *d->gameChoice
           << *LabelWidget::newWithText("Data File:", form)
           << *d->customDataFileName
           << *LabelWidget::newWithText(tr("Mods:"), form)
           << *d->packages;

    //LabelWidget *optionsLabel = LabelWidget::newWithText(_E(D) + tr("Game Options"), form);
    //optionsLabel->setFont("separator.label");
    //optionsLabel->margins().setTop("gap");
    //layout.setCellAlignment(Vector2i(0, layout.gridSize().y), ui::AlignLeft);
    //layout.append(*optionsLabel, 2);
    LabelWidget::appendSeparatorWithText("Game Options", form, &layout);

    layout << *LabelWidget::newWithText(tr("Starts in:"), form)
           << *d->autoStartMap
           << *LabelWidget::newWithText(tr("Skill:"), form)
           << *d->autoStartSkill;

    form->rule().setSize(layout.width(), layout.height());

    buttons().clear()
            << new DialogButtonItem(Id1 | Default | Accept, tr("Create"))
            << new DialogButtonItem(Reject);

    // The Create button is enabled when a valid name is entered.
    buttonWidget(Id1)->disable();

    updateLayout();
    d->updateDataFile();
    d->gameChanged();

    connect(d->gameChoice, &ChoiceWidget::selectionChanged, [this] () { d->gameChanged(); });
    connect(d->packages, &PackagesButtonWidget::packageSelectionChanged,
            [this] (QStringList) { d->updateMapList(); });
    connect(d->autoStartMap, &ChoiceWidget::selectionChanged, [this] ()
    {
        d->autoStartSkill->disable(d->autoStartMap->items().isEmpty() ||
                                   d->autoStartMap->selectedItem().data().toString().isEmpty());
    });
    connect(&editor(), &LineEditWidget::editorContentChanged,
            [this] () { d->checkValidProfileName(); });
}

GameProfile *CreateProfileDialog::makeProfile() const
{
    auto *prof = new GameProfile(profileName());
    prof->setUserCreated(true);
    applyTo(*prof);
    if (!prof->saveLocationId())
    {
        prof->createSaveLocation();
    }
    return prof;
}

void CreateProfileDialog::fetchFrom(GameProfile const &profile)
{
    editor().setText(profile.name());
    d->gameChoice->setSelected(d->gameChoice->items().findData(profile.gameId()));
    d->tempProfile->setCustomDataFile(profile.customDataFile());
    d->packages->setPackages(profile.packages());
    d->updateDataFile();
    d->autoStartMap->setSelected(d->autoStartMap->items().findData(profile.autoStartMap()));
    d->autoStartSkill->setSelected(d->autoStartSkill->items().findData(profile.autoStartSkill()));
    d->tempProfile->setSaveLocationId(profile.saveLocationId());
}

void CreateProfileDialog::applyTo(GameProfile &profile) const
{
    profile.setName(profileName());
    if (d->gameChoice->isValidSelection())
    {
        profile.setGame(d->gameChoice->selectedItem().data().toString());
    }
    profile.setCustomDataFile(d->tempProfile->customDataFile());
    profile.setUseGameRequirements(true);
    profile.setPackages(d->packages->packages());
    profile.setAutoStartMap(d->autoStartMap->selectedItem().data().toString());
    profile.setAutoStartSkill(d->autoStartSkill->selectedItem().data().toInt());
    profile.setSaveLocationId(profile.saveLocationId());
}

String CreateProfileDialog::profileName() const
{
    return editor().text().strip();
}

CreateProfileDialog *CreateProfileDialog::editProfile(String const &gameFamily,
                                                      GameProfile &profile)
{
    auto *dlg = new CreateProfileDialog(gameFamily);
    dlg->d->editing = true;
    dlg->d->oldName = profile.name();
    dlg->title().setText(tr("Edit Profile"));
    dlg->message().hide();
    dlg->buttonWidget(Id1)->setText(_E(b) + tr("OK"));
    dlg->fetchFrom(profile);
    dlg->updateLayout();
    return dlg;
}
