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
#include "GridLayout"
#include "SignalAction"
#include "DialogContentStylist"
#include "con_main.h"

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(RendererSettingsDialog)
{
    ChoiceWidget *appear;
    ButtonWidget *appearButton;
    CVarSliderWidget *fov;
    CVarToggleWidget *mirrorWeapon;
    CVarToggleWidget *multiLight;
    CVarToggleWidget *multiShiny;
    CVarToggleWidget *multiDetail;
    CVarChoiceWidget *rendTex;
    CVarChoiceWidget *wireframe;
    PopupWidget *devPopup;
    QScopedPointer<DialogContentStylist> stylist;
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

        area.add(mirrorWeapon = new CVarToggleWidget("rend-model-mirror-hud"));
        area.add(rendTex      = new CVarChoiceWidget("rend-tex"));
        area.add(multiLight   = new CVarToggleWidget("rend-light-multitex"));
        area.add(multiShiny   = new CVarToggleWidget("rend-model-shiny-multitex"));
        area.add(multiDetail  = new CVarToggleWidget("rend-tex-detail-multitex"));
        area.add(wireframe    = new CVarChoiceWidget("rend-dev-wireframe"));

        // Set up a separate popup for developer settings.
        self.add(devPopup = new PopupWidget);
        devPopup->set(devPopup->background().withSolidFillOpacity(1));

        GuiWidget *container = new GuiWidget;
        devPopup->setContent(container);
        stylist.reset(new DialogContentStylist(*container));

        container->add(bboxMobj   = new CVarToggleWidget("rend-dev-mobj-bbox"));
        container->add(bboxPoly   = new CVarToggleWidget("rend-dev-polyobj-bbox"));
        container->add(thinkerIds = new CVarToggleWidget("rend-dev-thinker-ids"));
        container->add(secIdx     = new CVarToggleWidget("rend-dev-sector-show-indices"));
        container->add(vertIdx    = new CVarToggleWidget("rend-dev-vertex-show-indices"));
        container->add(genIdx     = new CVarToggleWidget("rend-dev-generator-show-indices"));

        // Layout for the developer settings.
        Rule const &gap = self.style().rules().rule("gap");
        GridLayout layout(container->rule().left() + gap,
                          container->rule().top()  + gap);
        layout.setGridSize(1, 0);

        layout << *bboxMobj
               << *bboxPoly
               << *thinkerIds
               << *secIdx
               << *vertIdx
               << *genIdx;

        container->rule().setSize(layout.width()  + gap * 2,
                                  layout.height() + gap * 2);
    }

    void fetch()
    {
        /// @todo These widgets should be intelligent enough to fetch their
        /// cvar values when they need to....

        foreach(Widget *child, self.area().childWidgets() + devPopup->content().childWidgets())
        {
            if(CVarToggleWidget *w = child->maybeAs<CVarToggleWidget>())
            {
                w->updateFromCVar();
            }
            if(CVarChoiceWidget *w = child->maybeAs<CVarChoiceWidget>())
            {
                w->updateFromCVar();
            }
            if(CVarSliderWidget *w = child->maybeAs<CVarSliderWidget>())
            {
                w->updateFromCVar();
            }
        }
    }
};

RendererSettingsDialog::RendererSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Renderer Settings"));

    LabelWidget *appearLabel = LabelWidget::newWithText(tr("Appearance:"), &area());

    d->appear->setOpeningDirection(ui::Down);
    d->appear->items()
            << new ChoiceItem(tr("Default"), "");

    d->appearButton->setImage(style().images().image("gear"));
    d->appearButton->setSizePolicy(ui::Expand, ui::Expand);
    d->appearButton->setOverrideImageSize(style().fonts().font("default").height().valuei());

    d->appearButton->setAction(new SignalAction(this, SLOT(showAppearanceMenu())));

    LabelWidget *fovLabel = LabelWidget::newWithText(tr("Field of View:"), &area());

    d->mirrorWeapon->setText(tr("Mirror Player Weapon Model"));

    LabelWidget *multiLabel = LabelWidget::newWithText(tr("Multitexturing:"), &area());
    d->multiLight->setText(tr("Dynamic Lights"));
    d->multiShiny->setText(tr("3D Model Shiny Surfaces"));
    d->multiDetail->setText(tr("Surface Details"));

    LabelWidget *texLabel = LabelWidget::newWithText(tr("Surface Texturing:"), &area());
    d->rendTex->items()
            << new ChoiceItem(tr("Materials"),   1)
            << new ChoiceItem(tr("Plain white"), 0)
            << new ChoiceItem(tr("Plain gray"),  2);

    LabelWidget *wireLabel = LabelWidget::newWithText(tr("Draw as Wireframe:"), &area());
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
    layout << *appearLabel << *d->appear
           << *fovLabel   << *d->fov
           << Const(0)    << *d->mirrorWeapon
           << *multiLabel << *d->multiLight
           << Const(0)    << *d->multiShiny
           << Const(0)    << *d->multiDetail
           << *texLabel   << *d->rendTex
           << *wireLabel  << *d->wireframe;
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
    Con_SetFloat  ("rend-camera-fov", 95.f);
    Con_SetInteger("rend-model-mirror-hud", 0);
    Con_SetInteger("rend-tex", 1);
    Con_SetInteger("rend-light-multitex", 1);
    Con_SetInteger("rend-model-shiny-multitex", 1);
    Con_SetInteger("rend-tex-detail-multitex", 1);
    Con_SetInteger("rend-dev-wireframe", 0);

    Con_SetInteger("rend-dev-thinker-ids", 0);
    Con_SetInteger("rend-dev-mobj-bbox", 0);
    Con_SetInteger("rend-dev-polyobj-bbox", 0);
    Con_SetInteger("rend-dev-sector-show-indices", 0);
    Con_SetInteger("rend-dev-vertex-show-indices", 0);
    Con_SetInteger("rend-dev-generator-show-indices", 0);

    d->fetch();
}

void RendererSettingsDialog::showAppearanceMenu()
{
    PopupMenuWidget *popup = new PopupMenuWidget;
    popup->set(popup->background().withSolidFillOpacity(1));
    popup->menu().items()
            << new ActionItem(tr("Edit"))
            << new ActionItem(tr("Rename..."))
            << new Item(Item::Separator)
            << new ActionItem(tr("Add Duplicate..."))
            << new Item(Item::Separator)
            << new ActionItem(tr("Delete"));
    add(popup);

    ContextWidgetOrganizer const &org = popup->menu().organizer();

    // Enable or disable buttons depending on the selected profile.
    if(d->appear->selectedItem().data().toString().isEmpty())
    {
        // Default profile.
        org.itemWidget(0)->disable();
        org.itemWidget(1)->disable();
        org.itemWidget(5)->disable();
    }

    popup->setDeleteAfterDismissed(true);
    popup->setAnchorAndOpeningDirection(d->appearButton->rule(), ui::Down);
    popup->open();
}

void RendererSettingsDialog::showDeveloperPopup()
{
    d->devPopup->open();
}
