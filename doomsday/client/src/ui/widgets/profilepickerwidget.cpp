/** @file profilepickerwidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/profilepickerwidget.h"
#include "ui/widgets/popupmenuwidget.h"
#include "ui/dialogs/inputdialog.h"
#include "ui/clientwindow.h"

#include "SignalAction"

using namespace de;
using namespace ui;

static int const MAX_VISIBLE_PROFILE_NAME = 50;
static int const MAX_PROFILE_NAME = 100;

DENG_GUI_PIMPL(ProfilePickerWidget)
{
    SettingsRegister &settings;
    String description;
    ButtonWidget *button;

    Instance(Public *i, SettingsRegister& reg) : Base(i), settings(reg)
    {
        self.add(button = new ButtonWidget);
        button->setAction(new SignalAction(thisPublic, SLOT(openMenu())));

        updateStyle();
    }

    void updateStyle()
    {
        button->setSizePolicy(ui::Expand, ui::Expand);
        button->setImage(style().images().image("gear"));
        button->setOverrideImageSize(style().fonts().font("default").height().valuei());
    }

    void populate()
    {
        self.items().clear();

        foreach(String prof, settings.profiles())
        {
            self.items() << new ChoiceItem(prof.left(MAX_VISIBLE_PROFILE_NAME), prof);
        }

        // The items are alphabetically ordered.
        self.items().sort();

        self.setSelected(self.items().findData(settings.currentProfile()));
    }

    String currentProfile() const
    {
        return self.selectedItem().data().toString();
    }
};

ProfilePickerWidget::ProfilePickerWidget(SettingsRegister &settings, String const &description, String const &name)
    : ChoiceWidget(name), d(new Instance(this, settings))
{
    d->description = description;
    d->populate();

    // Attach the button next to the widget.
    d->button->margins().setAll(margins());
    d->button->rule()
            .setInput(Rule::Left, rule().right())
            .setInput(Rule::Top,  rule().top());

    connect(this, SIGNAL(selectionChangedByUser(uint)), this, SLOT(applySelectedProfile()));
}

ButtonWidget &ProfilePickerWidget::button()
{
    return *d->button;
}

void ProfilePickerWidget::updateStyle()
{
    ChoiceWidget::updateStyle();

    d->updateStyle();
}

void ProfilePickerWidget::openMenu()
{
    SettingsRegister &reg = d->settings;

    PopupMenuWidget *popup = new PopupMenuWidget;
    popup->items()
            << new ActionItem(tr("Edit"), new SignalAction(this, SLOT(edit())))
            << new ActionItem(tr("Rename..."), new SignalAction(this, SLOT(rename())))
            << new ui::Item(Item::Separator)
            << new ActionItem(tr("Duplicate..."), new SignalAction(this, SLOT(duplicate())))
            << new ui::Item(Item::Separator)
            << new ActionItem(tr("Reset to Defaults..."), new SignalAction(this, SLOT(reset())))
            << new ActionItem(tr("Delete..."), new SignalAction(this, SLOT(remove())));
    add(popup);

    ContextWidgetOrganizer const &org = popup->menu().organizer();

    // Enable or disable buttons depending on the selected profile.
    String selProf = selectedItem().data().toString();
    if(reg.isReadOnlyProfile(selProf))
    {
        // Read-only profiles can only be duplicated.
        org.itemWidget(0)->disable();
        org.itemWidget(1)->disable();
        org.itemWidget(5)->disable();
        org.itemWidget(6)->disable();
    }
    if(reg.profileCount() == 1)
    {
        // The last profile cannot be deleted.
        org.itemWidget(6)->disable();
    }
    if(root().window().hasSidebar())
    {
        // The sidebar is already open, so don't allow editing.
        org.itemWidget(0)->disable();
    }

    popup->setDeleteAfterDismissed(true);
    popup->setAnchorAndOpeningDirection(d->button->rule(), ui::Down);
    popup->open();
}

void ProfilePickerWidget::edit()
{
    emit profileEditorRequested();
}

void ProfilePickerWidget::rename()
{
    InputDialog *dlg = new InputDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText(tr("Renaming \"%1\"").arg(d->currentProfile()));
    dlg->message().setText(tr("Enter a new name for the %1 profile:").arg(d->description));
    dlg->defaultActionItem()->setLabel(tr("Rename Profile"));

    dlg->editor().setText(d->currentProfile());

    if(dlg->exec(root()))
    {
        String newName = dlg->editor().text().trimmed().left(MAX_PROFILE_NAME);
        if(!newName.isEmpty() && d->currentProfile() != newName)
        {
            if(d->settings.rename(newName))
            {
                ui::Item &item = items().at(selected());
                item.setLabel(newName.left(MAX_VISIBLE_PROFILE_NAME));
                item.setData(newName);

                // Keep the list sorted.
                items().sort();
                setSelected(items().findData(newName));
            }
            else
            {
                LOG_WARNING("Failed to rename profile to \"%s\"") << newName;
            }
        }
    }
}

void ProfilePickerWidget::duplicate()
{
    InputDialog *dlg = new InputDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText(tr("Duplicating \"%1\"").arg(d->currentProfile()));
    dlg->message().setText(tr("Enter a name for the new %1 profile:").arg(d->description));
    dlg->defaultActionItem()->setLabel(tr("Duplicate Profile"));

    if(dlg->exec(root()))
    {
        String newName = dlg->editor().text().trimmed().left(MAX_PROFILE_NAME);
        if(!newName.isEmpty())
        {
            if(d->settings.saveAsProfile(newName))
            {
                d->settings.setProfile(newName);

                items().append(new ChoiceItem(newName.left(MAX_VISIBLE_PROFILE_NAME), newName)).sort();
                setSelected(items().findData(newName));

                applySelectedProfile();
            }
            else
            {
                LOG_WARNING("Failed to duplicate current profile to create \"%s\"")
                        << newName;
            }
        }
    }
}

void ProfilePickerWidget::reset()
{
    MessageDialog *dlg = new MessageDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText(tr("Reset?"));
    dlg->message().setText(tr("Are you sure you want to reset the %1 profile %2 to the default values?")
                           .arg(d->description)
                           .arg(_E(b) + d->currentProfile() + _E(.)));

    dlg->buttons().items()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Reject)
            << new DialogButtonItem(DialogWidget::Accept, tr("Reset Profile"));

    if(dlg->exec(root()))
    {
        d->settings.resetToDefaults();
    }
}

void ProfilePickerWidget::remove()
{
    MessageDialog *dlg = new MessageDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText(tr("Delete?"));
    dlg->message().setText(
                tr("Are you sure you want to delete the %1 profile %2? This cannot be undone.")
                .arg(d->description)
                .arg(_E(b) + d->currentProfile() + _E(.)));
    dlg->buttons().items()
               << new DialogButtonItem(DialogWidget::Default | DialogWidget::Reject)
               << new DialogButtonItem(DialogWidget::Accept, tr("Delete Profile"));

    if(!dlg->exec(root())) return;

    // We've got the permission.
    String const profToDelete = d->currentProfile();

    items().remove(selected());

    // Switch to the new selection.
    applySelectedProfile();

    d->settings.deleteProfile(profToDelete);
}

void ProfilePickerWidget::applySelectedProfile()
{
    d->settings.setProfile(d->currentProfile());
}
