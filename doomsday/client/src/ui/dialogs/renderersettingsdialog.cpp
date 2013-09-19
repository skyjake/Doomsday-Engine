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
#include "ui/widgets/profilepickerwidget.h"
#include "ui/dialogs/inputdialog.h"
#include "GridLayout"
#include "SignalAction"
#include "DialogContentStylist"
#include "con_main.h"
#include "clientapp.h"

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(RendererSettingsDialog)
{
    ProfilePickerWidget *appear;
    CVarSliderWidget *fov;
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

        area.add(appear = new ProfilePickerWidget(ClientApp::rendererAppearanceSettings(),
                                                  tr("appearance")));
        appear->setOpeningDirection(ui::Down);

        area.add(fov = new CVarSliderWidget("rend-camera-fov"));
        fov->setPrecision(0);
        fov->setRange(Ranged(30, 160));

        area.add(precacheModels  = new CVarToggleWidget("rend-model-precache"));
        area.add(precacheSprites = new CVarToggleWidget("rend-sprite-precache"));
        area.add(multiLight      = new CVarToggleWidget("rend-light-multitex"));
        area.add(multiShiny      = new CVarToggleWidget("rend-model-shiny-multitex"));
        area.add(multiDetail     = new CVarToggleWidget("rend-tex-detail-multitex"));

        // Set up a separate popup for developer settings.
        self.add(devPopup = new PopupWidget);

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
};

RendererSettingsDialog::RendererSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Renderer Settings"));

    LabelWidget *appearLabel = LabelWidget::newWithText(tr("Appearance:"), &area());
    LabelWidget *fovLabel = LabelWidget::newWithText(tr("Field of View:"), &area());

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

    LabelWidget *capLabel = LabelWidget::newWithText(_E(1)_E(D) + tr("Behavior"), &area());
    capLabel->margins().setTop("gap");

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *appearLabel;

    // The profile button must be included in the layout.
    layout.append(*d->appear, d->appear->rule().width() + d->appear->button().rule().width());

    layout << *fovLabel << *d->fov;

    // Label for the tech caps.
    layout.setCellAlignment(Vector2i(0, 2), ui::AlignTopLeft);
    layout.append(*capLabel, 2);

    layout
           << *precacheLabel << *d->precacheModels
           << Const(0)       << *d->precacheSprites
           << *multiLabel    << *d->multiLight
           << Const(0)       << *d->multiShiny
           << Const(0)       << *d->multiDetail;

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())))
            << new DialogButtonItem(DialogWidget::Action | Id1,
                                    style().images().image("gauge"),
                                    new SignalAction(this, SLOT(showDeveloperPopup())));

    // Identifiers popup opens from the button.
    d->devPopup->setAnchorAndOpeningDirection(buttonWidget(Id1)->rule(), ui::Up);

    connect(this, SIGNAL(closed()), d->devPopup, SLOT(close()));
    connect(d->appear, SIGNAL(profileEditorRequested()), this, SLOT(editProfile()));

    d->fetch();
}

void RendererSettingsDialog::resetToDefaults()
{
    ClientApp::rendererSettings().resetToDefaults();

    d->fetch();
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
