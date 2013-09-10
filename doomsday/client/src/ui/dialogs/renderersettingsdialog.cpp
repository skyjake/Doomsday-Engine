/** @file renderersettingsdialog.cpp  Settings for the renderer.
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

#include "ui/dialogs/renderersettingsdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/editors/rendererappearanceeditor.h"
#include "ui/dialogs/inputdialog.h"
#include "GridLayout"
#include "SignalAction"
#include "DialogContentStylist"
#include "con_main.h"
#include "clientapp.h"

using namespace de;
using namespace ui;

static int const MAX_VISIBLE_PROFILE_NAME = 50;

DENG_GUI_PIMPL(RendererSettingsDialog)
{
    ChoiceWidget *appear;
    ButtonWidget *appearButton;
    CVarSliderWidget *fov;
    CVarToggleWidget *mirrorWeapon;
    CVarToggleWidget *precacheModels;
    CVarToggleWidget *precacheSprites;
    CVarToggleWidget *multiLight;
    CVarToggleWidget *multiShiny;
    CVarToggleWidget *multiDetail;

    // Developer settings.
    PopupWidget *devPopup;
    QScopedPointer<DialogContentStylist> stylist;
    CVarChoiceWidget *rendTex;
    CVarChoiceWidget *wireframe;
    CVarToggleWidget *bboxMobj;
    CVarToggleWidget *bboxPoly;
    CVarToggleWidget *thinkerIds;
    CVarToggleWidget *secIdx;
    CVarToggleWidget *vertIdx;
    CVarToggleWidget *genIdx;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        area.add(appear = new ChoiceWidget);
        area.add(appearButton = new ButtonWidget);

        area.add(fov = new CVarSliderWidget("rend-camera-fov"));
        fov->setPrecision(0);
        fov->setRange(Ranged(30, 160));

        area.add(mirrorWeapon    = new CVarToggleWidget("rend-model-mirror-hud"));
        area.add(precacheModels  = new CVarToggleWidget("rend-model-precache"));
        area.add(precacheSprites = new CVarToggleWidget("rend-sprite-precache"));
        area.add(multiLight      = new CVarToggleWidget("rend-light-multitex"));
        area.add(multiShiny      = new CVarToggleWidget("rend-model-shiny-multitex"));
        area.add(multiDetail     = new CVarToggleWidget("rend-tex-detail-multitex"));

        // Set up a separate popup for developer settings.
        self.add(devPopup = new PopupWidget);
        devPopup->set(devPopup->background().withSolidFillOpacity(1));

        GuiWidget *container = new GuiWidget;
        devPopup->setContent(container);
        stylist.reset(new DialogContentStylist(*container));

        LabelWidget *boundLabel = LabelWidget::newWithText(tr("Bounds:"), container);
        LabelWidget *idLabel    = LabelWidget::newWithText(tr("Identifiers:"), container);
        LabelWidget *texLabel   = LabelWidget::newWithText(tr("Surface Texturing:"), container);
        LabelWidget *wireLabel  = LabelWidget::newWithText(tr("Draw as Wireframe:"), container);

        container->add(bboxMobj   = new CVarToggleWidget("rend-dev-mobj-bbox"));
        container->add(bboxPoly   = new CVarToggleWidget("rend-dev-polyobj-bbox"));
        container->add(thinkerIds = new CVarToggleWidget("rend-dev-thinker-ids"));
        container->add(secIdx     = new CVarToggleWidget("rend-dev-sector-show-indices"));
        container->add(vertIdx    = new CVarToggleWidget("rend-dev-vertex-show-indices"));
        container->add(genIdx     = new CVarToggleWidget("rend-dev-generator-show-indices"));
        container->add(rendTex    = new CVarChoiceWidget("rend-tex"));
        container->add(wireframe  = new CVarChoiceWidget("rend-dev-wireframe"));

        // Layout for the developer settings.
        Rule const &gap = self.style().rules().rule("gap");
        GridLayout layout(container->rule().left() + gap,
                          container->rule().top()  + gap);
        layout.setGridSize(2, 0);
        layout.setColumnAlignment(0, ui::AlignRight);

        layout << *texLabel   << *rendTex
               << *wireLabel  << *wireframe
               << *boundLabel << *bboxMobj
               << Const(0)    << *bboxPoly
               << *idLabel    << *thinkerIds
               << Const(0)    << *secIdx
               << Const(0)    << *vertIdx
               << Const(0)    << *genIdx;

        container->rule().setSize(layout.width()  + gap * 2,
                                  layout.height() + gap * 2);
    }

    void fetch()
    {
        /// @todo These widgets should be intelligent enough to fetch their
        /// cvar values when they need to....

        foreach(Widget *child, self.area().childWidgets() + devPopup->content().childWidgets())
        {
            if(ICVarWidget *w = child->maybeAs<ICVarWidget>())
            {
                w->updateFromCVar();
            }
        }
    }

    String currentAppearance() const
    {
        return appear->selectedItem().data().toString();
    }
};

RendererSettingsDialog::RendererSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Renderer Settings"));

    LabelWidget *appearLabel = LabelWidget::newWithText(tr("Appearance:"), &area());

    d->appear->setOpeningDirection(ui::Down);

    // Populate the appearance profiles list.
    foreach(String prof, ClientApp::rendererAppearanceSettings().profiles())
    {
        d->appear->items() << new ChoiceItem(prof.left(MAX_VISIBLE_PROFILE_NAME), prof);
    }
    d->appear->items().sort();
    d->appear->setSelected(d->appear->items().findData(
            ClientApp::rendererAppearanceSettings().currentProfile()));
    connect(d->appear, SIGNAL(selectionChangedByUser(uint)), this, SLOT(applySelectedAppearance()));

    d->appearButton->setImage(style().images().image("gear"));
    d->appearButton->setSizePolicy(ui::Expand, ui::Expand);
    d->appearButton->setOverrideImageSize(style().fonts().font("default").height().valuei());

    d->appearButton->setAction(new SignalAction(this, SLOT(showAppearanceMenu())));

    LabelWidget *fovLabel = LabelWidget::newWithText(tr("Field of View:"), &area());

    d->mirrorWeapon->setText(tr("Mirror Player Weapon Model"));   

    LabelWidget *precacheLabel = LabelWidget::newWithText(tr("Precaching:"), &area());
    d->precacheModels->setText(tr("3D Models"));
    d->precacheSprites->setText(tr("Sprites"));

    LabelWidget *multiLabel = LabelWidget::newWithText(tr("Multitexturing:"), &area());
    d->multiLight->setText(tr("Dynamic Lights"));
    d->multiShiny->setText(tr("3D Model Shiny Surfaces"));
    d->multiDetail->setText(tr("Surface Details"));

    d->rendTex->items()
            << new ChoiceItem(tr("Materials"),   1)
            << new ChoiceItem(tr("Plain white"), 0)
            << new ChoiceItem(tr("Plain gray"),  2);

    d->wireframe->items()
            << new ChoiceItem(tr("Nothing"), 0)
            << new ChoiceItem(tr("Game world"), 1)
            << new ChoiceItem(tr("Game world and UI"), 2);

    // Developer labels.
    d->bboxMobj->setText(tr("Mobj Bounding Boxes"));
    d->bboxPoly->setText(tr("Polyobj Bounding Boxes"));
    d->thinkerIds->setText(tr("Thinker IDs"));
    d->secIdx->setText(tr("Sector Indices"));
    d->vertIdx->setText(tr("Vertex Indices"));
    d->genIdx->setText(tr("Particle Generator Indices"));

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *appearLabel;

    // The button is included in the layout.
    layout.append(*d->appear, d->appear->rule().width() + d->appearButton->rule().width());

    layout << *fovLabel      << *d->fov
           << Const(0)       << *d->mirrorWeapon
           << *precacheLabel << *d->precacheModels
           << Const(0)       << *d->precacheSprites
           << *multiLabel    << *d->multiLight
           << Const(0)       << *d->multiShiny
           << Const(0)       << *d->multiDetail;

    area().setContentSize(layout.width(), layout.height());

    // Attach the appearance button next to the choice.
    d->appearButton->rule()
            .setInput(Rule::Left, d->appear->rule().right())
            .setInput(Rule::Top,  d->appear->rule().top());


    buttons().items()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())))
            << new DialogButtonItem(DialogWidget::Action, tr("Developer"),
                                    new SignalAction(this, SLOT(showDeveloperPopup())));

    // Identifiers popup opens from the button.
    d->devPopup->setAnchorAndOpeningDirection(
                buttons().organizer().itemWidget(tr("Developer"))->rule(), ui::Up);
    connect(this, SIGNAL(closed()), d->devPopup, SLOT(close()));

    d->fetch();
}

void RendererSettingsDialog::resetToDefaults()
{
    ClientApp::rendererSettings().resetToDefaults();

    d->fetch();
}

void RendererSettingsDialog::showAppearanceMenu()
{
    SettingsRegister &reg = ClientApp::rendererAppearanceSettings();

    PopupMenuWidget *popup = new PopupMenuWidget;
    popup->set(popup->background().withSolidFillOpacity(1));
    popup->menu().items()
            << new ActionItem(tr("Edit"), new SignalAction(this, SLOT(editProfile())))
            << new ActionItem(tr("Rename..."), new SignalAction(this, SLOT(renameProfile())))
            << new Item(Item::Separator)
            << new ActionItem(tr("Add Duplicate..."), new SignalAction(this, SLOT(duplicateProfile())))
            << new Item(Item::Separator)
            << new ActionItem(tr("Reset to Defaults..."), new SignalAction(this, SLOT(resetProfile())))
            << new ActionItem(tr("Delete..."), new SignalAction(this, SLOT(deleteProfile())));
    add(popup);

    ContextWidgetOrganizer const &org = popup->menu().organizer();

    // Enable or disable buttons depending on the selected profile.
    String selProf = d->appear->selectedItem().data().toString();
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
    popup->setAnchorAndOpeningDirection(d->appearButton->rule(), ui::Down);
    popup->open();
}

void RendererSettingsDialog::showDeveloperPopup()
{
    d->devPopup->open();
}

void RendererSettingsDialog::editProfile()
{
    RendererAppearanceEditor *editor = new RendererAppearanceEditor;
    editor->open();

    ClientWindow::main().taskBar().closeConfigMenu();
}

void RendererSettingsDialog::renameProfile()
{
    InputDialog *dlg = new InputDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText(tr("Renaming \"%1\"").arg(d->currentAppearance()));
    dlg->message().setText(tr("Enter a new name for the appearance profile:"));
    dlg->defaultActionItem()->setLabel(tr("Rename Profile"));

    dlg->editor().setText(d->currentAppearance());

    if(dlg->exec(root()))
    {
        String clean = dlg->editor().text().trimmed().left(100);
        if(!clean.isEmpty() && d->currentAppearance() != clean)
        {
            if(ClientApp::rendererAppearanceSettings().rename(clean))
            {
                ui::Data &items = d->appear->items();

                ui::Item &item = items.at(d->appear->selected());
                item.setLabel(clean.left(MAX_VISIBLE_PROFILE_NAME));
                item.setData(clean);

                // Keep the list sorted.
                items.sort();
                d->appear->setSelected(items.findData(clean));
            }
            else
            {
                LOG_WARNING("Failed to rename profile to \"%s\"") << clean;
            }
        }
    }
}

void RendererSettingsDialog::duplicateProfile()
{
    InputDialog *dlg = new InputDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText(tr("Duplicating \"%1\"").arg(d->currentAppearance()));
    dlg->message().setText(tr("Enter a name for the new appearance profile:"));
    dlg->defaultActionItem()->setLabel(tr("Duplicate Profile"));

    if(dlg->exec(root()))
    {
        String clean = dlg->editor().text().trimmed().left(100);
        if(!clean.isEmpty())
        {
            SettingsRegister &reg = ClientApp::rendererAppearanceSettings();

            if(reg.saveAsProfile(clean))
            {
                reg.setProfile(clean);

                d->appear->items().append(new ChoiceItem(clean.left(MAX_VISIBLE_PROFILE_NAME), clean)).sort();
                d->appear->setSelected(d->appear->items().findData(clean));
            }
            else
            {
                LOG_WARNING("Failed to duplicate current profile to create \"%s\"")
                        << clean;
            }
        }
    }
}

void RendererSettingsDialog::resetProfile()
{
    MessageDialog *dlg = new MessageDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText(tr("Reset?").arg(d->currentAppearance()));
    dlg->message().setText(tr("Are you sure you want to reset the appearance profile %1 to the default values?")
                          .arg(_E(b) + d->currentAppearance() + _E(.)));

    dlg->buttons().items()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Reject)
            << new DialogButtonItem(DialogWidget::Accept, tr("Reset Profile"));

    if(dlg->exec(root()))
    {
        ClientApp::rendererAppearanceSettings().resetToDefaults();
    }
}

void RendererSettingsDialog::deleteProfile()
{
    MessageDialog *dlg = new MessageDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText(tr("Delete?"));
    dlg->message().setText(
                tr("Are you sure you want to delete the appearance profile %1? This cannot be undone.")
                .arg(_E(b) + d->currentAppearance() + _E(.)));
    dlg->buttons().items()
               << new DialogButtonItem(DialogWidget::Default | DialogWidget::Reject)
               << new DialogButtonItem(DialogWidget::Accept, tr("Delete Profile"));

    if(!dlg->exec(root())) return;

    // We've got the permission.
    String const profToDelete = d->currentAppearance();

    d->appear->items().remove(d->appear->selected());

    // Switch to the new selection.
    applySelectedAppearance();

    ClientApp::rendererAppearanceSettings().deleteProfile(profToDelete);
}

void RendererSettingsDialog::applySelectedAppearance()
{
    ClientApp::rendererAppearanceSettings().setProfile(d->currentAppearance());
}
