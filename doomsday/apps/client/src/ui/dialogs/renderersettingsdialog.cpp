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
#include "clientapp.h"
#include "ui/clientwindow.h"

#include <de/GridPopupWidget>
#include <de/GridLayout>
#include <de/SignalAction>
#include <de/DialogContentStylist>
#include <de/InputDialog>
#include <de/VariableSliderWidget>

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
    GridPopupWidget *devPopup;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        area.add(appear = new ProfilePickerWidget(ClientApp::renderSystem().appearanceSettings(),
                                                  tr("appearance"), "profile-picker"));
        appear->setOpeningDirection(ui::Down);

        area.add(fov = new CVarSliderWidget("rend-camera-fov"));
        fov->setPrecision(0);
        fov->setRange(Ranged(30, 160));

        // Set up a separate popup for developer settings.
        self.add(devPopup = new GridPopupWidget);

        CVarChoiceWidget *rendTex = new CVarChoiceWidget("rend-tex");
        rendTex->items()
                << new ChoiceItem(tr("Materials"),   1)
                << new ChoiceItem(tr("Plain white"), 0)
                << new ChoiceItem(tr("Plain gray"),  2);

        CVarChoiceWidget *wireframe = new CVarChoiceWidget("rend-dev-wireframe");
        wireframe->items()
                << new ChoiceItem(tr("Nothing"), 0)
                << new ChoiceItem(tr("Game world"), 1)
                << new ChoiceItem(tr("Game world and UI"), 2);

        precacheModels  = new CVarToggleWidget("rend-model-precache",       tr("3D Models"));
        precacheSprites = new CVarToggleWidget("rend-sprite-precache",      tr("Sprites"));
        multiLight      = new CVarToggleWidget("rend-light-multitex",       tr("Dynamic Lights"));
        multiShiny      = new CVarToggleWidget("rend-model-shiny-multitex", tr("3D Model Shiny Surfaces"));
        multiDetail     = new CVarToggleWidget("rend-tex-detail-multitex",  tr("Surface Details"));

        devPopup->addSeparatorLabel(tr("Behavior"));
        *devPopup << LabelWidget::newWithText(tr("Precaching:")) << precacheModels
                  << Const(0) << precacheSprites
                  << LabelWidget::newWithText(tr("Multitexturing:")) << multiLight
                  << Const(0) << multiShiny
                  << Const(0) << multiDetail;

        devPopup->addSeparatorLabel(tr("Diagnosis"));
        *devPopup << LabelWidget::newWithText(tr("Surface Texturing:"))
                  << rendTex
                  << LabelWidget::newWithText(tr("Draw as Wireframe:"))
                  << wireframe
                  << LabelWidget::newWithText(tr("Bounds:"))
                  << new CVarToggleWidget("rend-dev-mobj-bbox", tr("Mobj Bounding Boxes"))
                  << Const(0)
                  << new CVarToggleWidget("rend-dev-polyobj-bbox", tr("Polyobj Bounding Boxes"))
                  << LabelWidget::newWithText(tr("Identifiers:"))
                  << new CVarToggleWidget("rend-dev-thinker-ids", tr("Thinker IDs"))
                  << Const(0)
                  << new CVarToggleWidget("rend-dev-sector-show-indices", tr("Sector Indices"))
                  << Const(0)
                  << new CVarToggleWidget("rend-dev-vertex-show-indices", tr("Vertex Indices"))
                  << Const(0)
                  << new CVarToggleWidget("rend-dev-generator-show-indices", tr("Particle Generator Indices"));

        devPopup->commit();
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
    appearLabel->setName("appearance-label"); // for lookup from tutorial
    LabelWidget *fovLabel = LabelWidget::newWithText(tr("Field of View:"), &area());

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *appearLabel;

    // The profile button must be included in the layout.
    layout.append(*d->appear, d->appear->rule().width() + d->appear->button().rule().width());

    layout << *fovLabel << *d->fov;

    // Slider for modifying the global pixel density factor. This allows slower
    // GPUs to compensate for large resolutions.
    {
        auto *pd = new VariableSliderWidget(App::config("render.pixelDensity"), Ranged(0, 1), .05);
        pd->setPrecision(2);
        area().add(pd);

        layout << *LabelWidget::newWithText(tr("Pixel Density:"), &area()) << *pd;
    }

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
    ClientApp::renderSystem().settings().resetToDefaults();

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
