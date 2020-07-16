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

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/gameprofiles.h>
#include <doomsday/res/databundle.h>

#include <de/auxbuttonwidget.h>
#include <de/callbackaction.h>
#include <de/choicewidget.h>
#include <de/dialogcontentstylist.h>
#include <de/foldpanelwidget.h>
#include <de/gridlayout.h>
#include <de/packageloader.h>
#include <de/persistentstate.h>
#include <de/scripting/scriptedinfo.h>
#include <de/sliderwidget.h>
#include <de/textvalue.h>
#include <de/togglewidget.h>

using namespace de;

DE_GUI_PIMPL(CreateProfileDialog)
, DE_OBSERVES(ToggleWidget, Toggle)
, DE_OBSERVES(SliderWidget, Value)
{
    GuiWidget *form;
    ChoiceWidget *gameChoice;
    PackagesButtonWidget *packages;
    ChoiceWidget *autoStartMap;
    ChoiceWidget *autoStartSkill;
    AuxButtonWidget *customDataFileName;
    ui::ListData customDataFileActions;
    SafeWidgetPtr<PackagesWidget> customPicker;
    FoldPanelWidget *optionsFold;
    GuiWidget *optionsBase;
    DialogContentStylist stylist;
    bool editing = false;
    String oldName;
    std::unique_ptr<GameProfile> tempProfile; // Used only internally.

    Impl(Public *i) : Base(i) {}

    void populateOptions()
    {
        optionsBase->clearTree();

        GridLayout layout(optionsBase->rule().left(), optionsBase->rule().top());
        layout.setGridSize(2, 0);
        layout.setColumnAlignment(0, ui::AlignRight);
        const auto &opts = tempProfile->game()[Game::DEF_OPTIONS].valueAsRecord().members();
        if (opts.isEmpty())
        {
            return;
        }
        StringList keys =
            map<StringList>(opts, [](const std::pair<String, Variable *> &i) { return i.first; });
        // Alphabetic order based on the label.
        std::sort(keys.begin(), keys.end(), [&opts](const String &k1, const String &k2) {
            return opts[k1]->valueAsRecord().gets("label").compareWithoutCase(
                       opts[k2]->valueAsRecord().gets("label")) < 0;
        });
        for (const auto &key : keys)
        {
            const Record &optDef   = opts[key]->valueAsRecord();
            const String  optType  = optDef["type"];
            const String  optLabel = optDef["label"];

            if (optType == "boolean")
            {
                auto *tog = new ToggleWidget;
                optionsBase->add(tog);
                tog->setText(optLabel);
                tog->setActive(ScriptedInfo::isTrue(tempProfile->optionValue(key)));
                tog->objectNamespace().set("option", key);
                tog->audienceForToggle() += this;
                layout << Const(0) << *tog;
            }
            else if (optType == "number")
            {
                auto *slider = new SliderWidget;
                optionsBase->add(slider);
                slider->setRange(Ranged(optDef.getd("min"), optDef.getd("max")));
                const double step = optDef.getd("step", 1.0);
                slider->setStep(step);
                slider->setValue(tempProfile->optionValue(key).asNumber());
                slider->setPrecision(step < 1 ? 1 : 0);
                slider->objectNamespace().set("option", key);
                slider->audienceForValue() += this;
                layout << *LabelWidget::newWithText(optLabel + ":", optionsBase)
                       << *slider;
            }
        }
        optionsBase->rule().setSize(layout);
    }

    void toggleStateChanged(ToggleWidget &widget) override
    {
        tempProfile->setOptionValue(widget["option"], NumberValue(widget.isActive()));
    }

    void sliderValueChanged(SliderWidget &widget, double val) override
    {
        tempProfile->setOptionValue(widget["option"], NumberValue(val));
    }

    void checkValidProfileName()
    {
        bool valid = false;

        const String entry = self().profileName();
        if (!entry.isEmpty())
        {
            if (editing && !oldName.compareWithoutCase(entry))
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
            gameChoice->selectedItem().data().asText().isEmpty()) 
        {
            valid = false;
        }
        self().buttonWidget(Id1)->enable(valid);
    }

    void gameChanged()
    {
        if (gameChoice->isValidSelection())
        {
            tempProfile->setGame(gameChoice->selectedItem().data().asText());
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
            oldChoice = autoStartMap->selectedItem().data().asText();
        }

        mapItems.clear();
        mapItems << new ChoiceItem("Title screen", TextValue());

        // Find out all the required and selected packages.
        StringList packageIds;
        if (gameChoice->isValidSelection())
        {
            packageIds += tempProfile->allRequiredPackages();
        }

        // Create menu items for the Start Map choice.
        for (auto i = packageIds.rbegin(); i != packageIds.rend(); ++i)
        {
            if (const File *pkgFile = PackageLoader::get().select(*i))
            {
                const DataBundle *bundle = maybeAs<DataBundle>(pkgFile->target());
                if (!bundle || !bundle->lumpDirectory())
                {
                    continue;
                }

                const auto maps = bundle->lumpDirectory()->findMapLumpNames();
                if (!maps.isEmpty())
                {
                    mapItems << new ui::Item(ui::Item::Separator);

                    const String wadName = Package::metadata(*pkgFile).gets(Package::VAR_TITLE);
                    for (const String &mapId : maps)
                    {
                        // Only show each map identifier once; only the last lump can
                        // be loaded.
                        if (mapItems.findData(TextValue(mapId)) == ui::Data::InvalidPos)
                        {
                            mapItems << new ChoiceItem(Stringf("%s  " _E(s) _E(C) "%s",
                                                                      mapId.c_str(),
                                                                      wadName.c_str()),
                                                       TextValue(mapId));
                        }
                    }
                }
            }
        }

        const auto pos = mapItems.findData(TextValue(oldChoice));
        autoStartMap->setSelected(pos != ui::Data::InvalidPos? pos : 0);
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

CreateProfileDialog::CreateProfileDialog(const String &gameFamily)
    : InputDialog("create-profile")
    , d(new Impl(this))
{
    title()  .setText("New Profile");
    message().setText("Enter a name for the new game profile.");

    // MessageDialog applies a layout on its area contents, so we'll have our own parent widget
    // for our widgets.
    {
        auto *form = new GuiWidget;
        d->form = form;
        d->stylist.setContainer(*form);
        area().add(form);

        // Populate games list.
        form->add(d->gameChoice = new ChoiceWidget);
        DoomsdayApp::games().forAll([this, &gameFamily] (Game &game)
        {
            if (game.family() == gameFamily)
            {
                const char *labelColor = (!game.isPlayable() ? _E(F) : "");
                d->gameChoice->items() << new ChoiceItem(labelColor + game.title(), TextValue(game.id()));
            }
            return LoopContinue;
        });
        d->gameChoice->items().sort();

        if (d->gameChoice->items().isEmpty())
        {
        d->gameChoice->items() << new ChoiceItem("No playable games", TextValue());
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
                d->customPicker->searchTermsEditor().setText("gamedata");
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
        d->packages->setNoneLabel("None");
        d->tempProfile.reset(new GameProfile);
        d->packages->setGameProfile(*d->tempProfile);

        GridLayout layout(form->rule().left(), form->rule().top() + rule("dialog.gap"));
        layout.setGridSize(2, 0);
        layout.setColumnAlignment(0, ui::AlignRight);
        layout << *LabelWidget::newWithText("Game:", form)
               << *d->gameChoice
               << *LabelWidget::newWithText("Data File:", form)
               << *d->customDataFileName
               << *LabelWidget::newWithText("Mods:", form)
               << *d->packages;
        form->rule().setSize(layout);
    }

    // Foldable panel for the Launch Options.
    {
        auto *launchOptions =
            FoldPanelWidget::makeOptionsGroup("launch-options", "Launch Options", &area());

        launchOptions->title().margins().setTop(rule("gap") + d->form->margins().bottom());

        auto *form = new GuiWidget;
        launchOptions->setContent(form);

        // Auto start map.
        form->add(d->autoStartMap = new ChoiceWidget);
        d->autoStartMap->popup().menu().enableIndicatorDraw(true);
        d->autoStartMap->items() << new ChoiceItem("Title screen", TextValue());

        // Auto start skill.
        form->add(d->autoStartSkill = new ChoiceWidget);
        d->autoStartSkill->items()
            << new ChoiceItem("Novice",    NumberValue(1))
            << new ChoiceItem("Easy",      NumberValue(2))
            << new ChoiceItem("Normal",    NumberValue(3))
            << new ChoiceItem("Hard",      NumberValue(4))
            << new ChoiceItem("Nightmare", NumberValue(5));
        d->autoStartSkill->disable();
        d->autoStartSkill->setSelected(2);

        GridLayout layout(form->rule().left(), form->rule().top());
        layout.setGridSize(2, 0);
        layout.setColumnAlignment(0, ui::AlignRight);
        layout << *LabelWidget::newWithText("Starts in:", form) << *d->autoStartMap
               << *LabelWidget::newWithText("Skill:", form) << *d->autoStartSkill;
        form->rule().setSize(layout);
//        optionsLabel->setFont("separator.label");
//        optionsLabel->margins().setTop("gap");
        layout.setCellAlignment(Vec2i(0, layout.gridSize().y), ui::AlignLeft);
    }

    // Additional options defined by the game.
    {
        d->optionsFold =
            FoldPanelWidget::makeOptionsGroup("gameplay-options", "Gameplay Options", &area());

        d->optionsBase = new GuiWidget;
        d->optionsFold->setContent(d->optionsBase);
    }

    buttons().clear()
            << new DialogButtonItem(Id1 | Default | Accept, "Create")
            << new DialogButtonItem(Reject);

    // The Create button is enabled when a valid name is entered.
    buttonWidget(Id1)->disable();

    updateLayout(IncludeHidden);
    d->updateDataFile();
    d->gameChanged();
    d->populateOptions();

    d->gameChoice->audienceForSelection() += [this](){ d->gameChanged(); };
    d->packages->audienceForSelection() += [this](){ d->updateMapList(); };
    d->autoStartMap->audienceForSelection() += [this]() {
        d->autoStartSkill->disable(d->autoStartMap->items().isEmpty() ||
                                   d->autoStartMap->selectedItem().data().asText().isEmpty());
    };
    editor().audienceForContentChange() += [this]() { d->checkValidProfileName(); };
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

void CreateProfileDialog::fetchFrom(const GameProfile &profile)
{
    editor().setText(profile.name());
    d->gameChoice->setSelected(d->gameChoice->items().findData(TextValue(profile.gameId())));
    d->tempProfile->setCustomDataFile(profile.customDataFile());
    d->packages->setPackages(profile.packages());
    d->updateDataFile();
    d->autoStartMap->setSelected(d->autoStartMap->items().findData(TextValue(profile.autoStartMap())));
    d->autoStartSkill->setSelected(d->autoStartSkill->items().findData(NumberValue(profile.autoStartSkill())));
    d->tempProfile->setSaveLocationId(profile.saveLocationId());
    d->tempProfile->objectNamespace() = profile.objectNamespace();
    d->populateOptions();
}

void CreateProfileDialog::applyTo(GameProfile &profile) const
{
    profile.setName(profileName());
    if (d->gameChoice->isValidSelection())
    {
        profile.setGame(d->gameChoice->selectedItem().data().asText());
    }
    profile.setCustomDataFile(d->tempProfile->customDataFile());
    profile.setUseGameRequirements(true);
    profile.setPackages(d->packages->packages());
    profile.setAutoStartMap(d->autoStartMap->selectedItem().data().asText());
    profile.setAutoStartSkill(d->autoStartSkill->selectedItem().data().asInt());
    profile.setSaveLocationId(profile.saveLocationId());
    profile.objectNamespace() = d->tempProfile->objectNamespace();
}

String CreateProfileDialog::profileName() const
{
    return editor().text().strip();
}

void CreateProfileDialog::operator>>(PersistentState &toState) const
{
    auto &ps = toState.objectNamespace();
    auto &launch = find("launch-options")->as<FoldPanelWidget>();
    ps.set(name().concatenateMember(launch.name()) + ".open", launch.isOpen());
    ps.set(name().concatenateMember(d->optionsFold->name()) + ".open",
           d->optionsFold->isOpen());
}

void CreateProfileDialog::operator<<(const PersistentState &fromState)
{
    auto &ps = fromState.objectNamespace();
    auto &launch = find("launch-options")->as<FoldPanelWidget>();
    if (ps.getb(name().concatenateMember(launch.name()) + ".open", true))
    {
        launch.open();
    }
    else
    {
        launch.close(0.0);
    }
    if (ps.getb(name().concatenateMember(d->optionsFold->name()) + ".open", false))
    {
        d->optionsFold->open();
    }
    else
    {
        d->optionsFold->close(0.0);
    }
}

CreateProfileDialog *CreateProfileDialog::editProfile(const String &gameFamily,
                                                      GameProfile &profile)
{
    auto *dlg = new CreateProfileDialog(gameFamily);
    dlg->d->editing = true;
    dlg->d->oldName = profile.name();
    dlg->title().setText("Edit Profile");
    delete &dlg->message();
    dlg->buttonWidget(Id1)->setText(_E(b) "OK");
    dlg->fetchFrom(profile);
    dlg->updateLayout(IncludeHidden);
    return dlg;
}
