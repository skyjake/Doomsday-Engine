/** @file createprofiledialog.cpp
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

#include "ui/dialogs/createprofiledialog.h"
#include "ui/widgets/packagesbuttonwidget.h"

#include <doomsday/DoomsdayApp>
#include <doomsday/Games>
#include <doomsday/GameProfiles>

#include <de/ChoiceWidget>
#include <de/GridLayout>
#include <de/DialogContentStylist>

using namespace de;

DENG_GUI_PIMPL(CreateProfileDialog)
{
    ChoiceWidget *gameChoice;
    PackagesButtonWidget *packages;
    DialogContentStylist stylist;
    bool editing = false;
    String oldName;

    Instance(Public *i) : Base(i) {}

    void checkValidProfileName()
    {
        bool valid = false;

        String const entry = self.profileName();
        if(!entry.isEmpty())
        {
            if(editing && oldName == entry)
            {
                valid = true;
            }
            else
            {
                // Must be a new, unique name.
                valid = DoomsdayApp::gameProfiles().forAll([this, &entry] (GameProfile &prof) {
                    if(!entry.compareWithoutCase(prof.name())) {
                        return LoopAbort;
                    }
                    return LoopContinue;
                }) == LoopContinue;
            }
        }

        // A game must be selected, too.
        if(!gameChoice->isValidSelection()) valid = false;

        self.buttonWidget(Id1)->enable(valid);
    }
};

CreateProfileDialog::CreateProfileDialog(String const &gameFamily)
    : InputDialog("create-profile")
    , d(new Instance(this))
{
    title()  .setText(tr("New Profile"));
    message().setText(tr("Enter a name for the new game profile. Only unique names are allowed."));

    auto *form = new GuiWidget;
    d->stylist.setContainer(*form);
    area().add(form);

    // Populate games list.
    form->add(d->gameChoice = new ChoiceWidget);
    DoomsdayApp::games().forAll([this, &gameFamily] (Game &game)
    {
        if(game.isPlayable() && game.family() == gameFamily)
        {
            d->gameChoice->items() << new ChoiceItem(game.title(), game.id());
        }
        return LoopContinue;
    });
    d->gameChoice->items().sort();

    // Packages selection.
    form->add(d->packages = new PackagesButtonWidget);
    d->packages->setNoneLabel(tr("None"));

    GridLayout layout(form->rule().left(), form->rule().top() + rule("dialog.gap"));
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *LabelWidget::newWithText(tr("Game:"), form)
           << *d->gameChoice
           << *LabelWidget::newWithText(tr("Packages:"), form)
           << *d->packages;

    form->rule().setSize(layout.width(), layout.height());

    buttons().clear()
            << new DialogButtonItem(Id1 | Default | Accept, tr("Create"))
            << new DialogButtonItem(Reject);

    // The Create button is enabled when a valid name is entered.
    buttonWidget(Id1)->disable();

    updateLayout();

    connect(&editor(), &LineEditWidget::editorContentChanged,
            [this] () { d->checkValidProfileName(); });
}

GameProfile *CreateProfileDialog::makeProfile() const
{
    auto *prof = new GameProfile(profileName());
    prof->setUserCreated(true);
    applyTo(*prof);
    return prof;
}

void CreateProfileDialog::fetchFrom(GameProfile const &profile)
{
    editor().setText(profile.name());
    d->gameChoice->setSelected(d->gameChoice->items().findData(profile.game()));
    d->packages->setPackages(profile.packages());
}

void CreateProfileDialog::applyTo(GameProfile &profile) const
{
    profile.setName(profileName());
    profile.setGame(d->gameChoice->selectedItem().data().toString());
    profile.setPackages(d->packages->packages());
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
