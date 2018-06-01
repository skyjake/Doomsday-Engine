/** @file profilepickerwidget.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/clientwindow.h"

#include <de/SignalAction>
#include <de/PopupMenuWidget>
#include <de/InputDialog>

using namespace de;
using namespace de::ui;

static int const MAX_VISIBLE_PROFILE_NAME = 50;
static int const MAX_PROFILE_NAME = 100;

DE_GUI_PIMPL(ProfilePickerWidget)
{
    ConfigProfiles &settings;
    String description;
    PopupButtonWidget *button;
    bool invertedPopups = false;

    Impl(Public *i, ConfigProfiles& reg)
        : Base(i)
        , settings(reg)
        , button(0)
    {
        self().add(button = new PopupButtonWidget);
        button->setOpener([this] (PopupWidget *) {
            self().openMenu();
        });

        updateStyle();
    }

    void updateStyle()
    {
        button->setSizePolicy(ui::Expand, ui::Expand);
        button->setImage(style().images().image("gear"));
        button->setOverrideImageSize(style().fonts().font("default").height());
    }

    void populate()
    {
        self().items().clear();

        foreach (String prof, settings.profiles())
        {
            self().items() << new ChoiceItem(prof.left(MAX_VISIBLE_PROFILE_NAME), prof);
        }

        // The items are alphabetically ordered.
        self().items().sort();

        self().setSelected(self().items().findData(settings.currentProfile()));
    }

    String currentProfile() const
    {
        return self().selectedItem().data().toString();
    }
};

ProfilePickerWidget::ProfilePickerWidget(ConfigProfiles &settings, String const &description, String const &name)
    : ChoiceWidget(name), d(new Impl(this, settings))
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

void ProfilePickerWidget::useInvertedStyleForPopups()
{
    d->invertedPopups = true;
    popup().useInfoStyle();
}

void ProfilePickerWidget::updateStyle()
{
    ChoiceWidget::updateStyle();

    d->updateStyle();
}

void ProfilePickerWidget::openMenu()
{
    ConfigProfiles &reg = d->settings;

    auto *menu = new PopupMenuWidget;
    menu->setAllowDirectionFlip(false);
    menu->setDeleteAfterDismissed(true);
    if (d->invertedPopups) menu->useInfoStyle();
    d->button->setPopup(*menu, ui::Down);
    menu->items()
            << new ActionItem(tr("Edit"), new SignalAction(this, SLOT(edit())))
            << new ActionItem(tr("Rename..."), new SignalAction(this, SLOT(rename())))
            << new ui::Item(Item::Separator)
            << new ActionItem(tr("Duplicate..."), new SignalAction(this, SLOT(duplicate())))
            << new ui::Item(Item::Separator)
            << new ActionItem(tr("Reset to Defaults..."), new SignalAction(this, SLOT(reset())))
            << new ActionItem(style().images().image("close.ring"), tr("Delete..."),
                              new SignalAction(this, SLOT(remove())));
    add(menu);

    ChildWidgetOrganizer const &org = menu->menu().organizer();

    // Enable or disable buttons depending on the selected profile.
    String selProf = selectedItem().data().toString();
    if (reg.find(selProf).isReadOnly())
    {
        // Read-only profiles can only be duplicated.
        menu->items().at(0).setLabel("View");
        org.itemWidget(1)->disable();
        org.itemWidget(5)->disable();
        org.itemWidget(6)->disable();
    }
    if (reg.count() == 1)
    {
        // The last profile cannot be deleted.
        org.itemWidget(6)->disable();
    }
    if (root().window().as<ClientWindow>().hasSidebar())
    {
        // The sidebar is already open, so don't allow editing.
        org.itemWidget(0)->disable();
    }

    menu->open();
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

    if (dlg->exec(root()))
    {
        String newName = dlg->editor().text().trimmed().left(MAX_PROFILE_NAME);
        if (!newName.isEmpty() && d->currentProfile() != newName)
        {
            if (d->settings.rename(newName))
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

    if (dlg->exec(root()))
    {
        String newName = dlg->editor().text().trimmed().left(MAX_PROFILE_NAME);
        if (!newName.isEmpty())
        {
            if (d->settings.saveAsProfile(newName))
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

    dlg->buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Reject)
            << new DialogButtonItem(DialogWidget::Accept, tr("Reset Profile"));

    if (dlg->exec(root()))
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
    dlg->buttons()
               << new DialogButtonItem(DialogWidget::Default | DialogWidget::Reject)
               << new DialogButtonItem(DialogWidget::Accept, tr("Delete Profile"));

    if (!dlg->exec(root())) return;

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
