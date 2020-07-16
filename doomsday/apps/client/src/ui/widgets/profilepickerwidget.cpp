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

#include <de/popupmenuwidget.h>
#include <de/inputdialog.h>
#include <de/textvalue.h>

using namespace de;
using namespace de::ui;

static constexpr CharPos MAX_VISIBLE_PROFILE_NAME{50};
static constexpr CharPos MAX_PROFILE_NAME{100};

DE_GUI_PIMPL(ProfilePickerWidget)
{
    ConfigProfiles &   settings;
    String             description;
    PopupButtonWidget *button;
    bool               invertedPopups = false;

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

        for (const String &prof : settings.profiles())
        {
            self().items() << new ChoiceItem(prof.left(MAX_VISIBLE_PROFILE_NAME), TextValue(prof));
        }

        // The items are alphabetically ordered.
        self().items().sort();

        self().setSelected(self().items().findData(TextValue(settings.currentProfile())));
    }

    String currentProfile() const
    {
        return self().selectedItem().data().asText();
    }

    DE_PIMPL_AUDIENCES(ProfileChange, EditorRequest)
};

DE_AUDIENCE_METHODS(ProfilePickerWidget, ProfileChange, EditorRequest)

ProfilePickerWidget::ProfilePickerWidget(ConfigProfiles &settings, const String &description, const String &name)
    : ChoiceWidget(name), d(new Impl(this, settings))
{
    d->description = description;
    d->populate();

    // Attach the button next to the widget.
    d->button->margins().setAll(margins());
    d->button->rule()
            .setInput(Rule::Left, rule().right())
            .setInput(Rule::Top,  rule().top());

    audienceForUserSelection() += [this](){ applySelectedProfile(); };
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
            << new ActionItem("Edit", [this](){ edit(); })
            << new ActionItem("Rename...", [this](){ rename(); })
            << new ui::Item(Item::Separator)
            << new ActionItem("Duplicate...", [this](){ duplicate(); })
            << new ui::Item(Item::Separator)
            << new ActionItem("Reset to Defaults...", [this](){ reset(); })
            << new ActionItem(style().images().image("close.ring"), "Delete...",
                              [this](){ remove(); });
    add(menu);

    const ChildWidgetOrganizer &org = menu->menu().organizer();

    // Enable or disable buttons depending on the selected profile.
    String selProf = selectedItem().data().asText();
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
    DE_NOTIFY(EditorRequest, i)
    {
        i->profileEditorRequested();
    }
}

void ProfilePickerWidget::rename()
{
    InputDialog *dlg = new InputDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText(Stringf("Renaming \"%s\"", d->currentProfile().c_str()));
    dlg->message().setText(Stringf("Enter a new name for the %s profile:", d->description.c_str()));
    dlg->defaultActionItem()->setLabel("Rename Profile");

    dlg->editor().setText(d->currentProfile());

    if (dlg->exec(root()))
    {
        String newName = dlg->editor().text().strip().left(MAX_PROFILE_NAME);
        if (!newName.isEmpty() && d->currentProfile() != newName)
        {
            if (d->settings.rename(newName))
            {
                ui::Item &item = items().at(selected());
                item.setLabel(newName.left(MAX_VISIBLE_PROFILE_NAME));
                item.setData(TextValue(newName));

                // Keep the list sorted.
                items().sort();
                setSelected(items().findData(TextValue(newName)));
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
    dlg->title().setText(Stringf("Duplicating \"%s\"", d->currentProfile().c_str()));
    dlg->message().setText(Stringf("Enter a name for the new %s profile:", d->description.c_str()));
    dlg->defaultActionItem()->setLabel("Duplicate Profile");

    if (dlg->exec(root()))
    {
        String newName = dlg->editor().text().strip().left(MAX_PROFILE_NAME);
        if (!newName.isEmpty())
        {
            if (d->settings.saveAsProfile(newName))
            {
                d->settings.setProfile(newName);

                items()
                    .append(new ChoiceItem(newName.left(MAX_VISIBLE_PROFILE_NAME), TextValue(newName)))
                    .sort();
                setSelected(items().findData(TextValue(newName)));

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
    dlg->title().setText("Reset?");
    dlg->message().setText(
        Stringf("Are you sure you want to reset the %s profile %s to the default values?",
                       d->description.c_str(),
                       (_E(b) + d->currentProfile() + _E(.)).c_str()));

    dlg->buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Reject)
            << new DialogButtonItem(DialogWidget::Accept, "Reset Profile");

    if (dlg->exec(root()))
    {
        d->settings.resetToDefaults();
    }
}

void ProfilePickerWidget::remove()
{
    MessageDialog *dlg = new MessageDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText("Delete?");
    dlg->message().setText(
        Stringf("Are you sure you want to delete the %s profile %s? This cannot be undone.",
                       d->description.c_str(),
                       (_E(b) + d->currentProfile() + _E(.)).c_str()));
    dlg->buttons()
               << new DialogButtonItem(DialogWidget::Default | DialogWidget::Reject)
               << new DialogButtonItem(DialogWidget::Accept, "Delete Profile");

    if (!dlg->exec(root())) return;

    // We've got the permission.
    const String profToDelete = d->currentProfile();

    items().remove(selected());

    // Switch to the new selection.
    applySelectedProfile();

    d->settings.deleteProfile(profToDelete);
}

void ProfilePickerWidget::applySelectedProfile()
{
    d->settings.setProfile(d->currentProfile());
}
