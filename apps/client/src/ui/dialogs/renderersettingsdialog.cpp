/** @file renderersettingsdialog.cpp  Settings for the renderer.
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

#include "ui/dialogs/renderersettingsdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/editors/rendererappearanceeditor.h"
#include "ui/widgets/profilepickerwidget.h"
#include "ui/clientwindow.h"
#include "render/rendersystem.h"
#include "gl/gl_texmanager.h"
#include "clientapp.h"

#include <doomsday/console/exec.h>

#include <de/gridpopupwidget.h>
#include <de/gridlayout.h>
#include <de/dialogcontentstylist.h>
#include <de/inputdialog.h>
#include <de/variablesliderwidget.h>
#include <de/variabletogglewidget.h>

using namespace de;
using namespace de::ui;

DE_GUI_PIMPL(RendererSettingsDialog)
{
    ProfilePickerWidget *appear;
    CVarSliderWidget *fov;
    VariableToggleWidget *enableExtWithPWADs;
    VariableToggleWidget *disableExtTextures;
    VariableToggleWidget *disableExtPatches;
    CVarToggleWidget *precacheModels;
    CVarToggleWidget *precacheSprites;

    // Developer settings.
    GridPopupWidget *devPopup;

    bool texSettingsToggled = false;

    Impl(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self().area();

        area.add(appear = new ProfilePickerWidget(ClientApp::render().appearanceSettings(),
                                                  "appearance",
                                                  "profile-picker"));
        appear->setOpeningDirection(ui::Down);

        area.add(fov = new CVarSliderWidget("rend-camera-fov"));
        fov->setPrecision(0);
        fov->setRange(Ranged(30, 160));

        area.add(enableExtWithPWADs = new VariableToggleWidget("Use with PWADs",       App::config("resource.highResWithPWAD")));
        area.add(disableExtTextures = new VariableToggleWidget("Disable for Textures", App::config("resource.noHighResTex")));
        area.add(disableExtPatches  = new VariableToggleWidget("Disable for Patches",  App::config("resource.noHighResPatches")));

        // Set up a separate popup for developer settings.
        self().add(devPopup = new GridPopupWidget);

        CVarChoiceWidget *rendTex = new CVarChoiceWidget("rend-tex");
        rendTex->items()
                << new ChoiceItem("Materials",   1)
                << new ChoiceItem("Plain white", 0)
                << new ChoiceItem("Plain gray",  2);

        CVarChoiceWidget *wireframe = new CVarChoiceWidget("rend-dev-wireframe");
        wireframe->items()
                << new ChoiceItem("Nothing", 0)
                << new ChoiceItem("Game world", 1)
                << new ChoiceItem("Game world and UI", 2);

        precacheModels  = new CVarToggleWidget("rend-model-precache",  "3D Models");
        precacheSprites = new CVarToggleWidget("rend-sprite-precache", "Sprites");

        devPopup->addSeparatorLabel("Behavior");
        *devPopup << LabelWidget::newWithText("Precaching:") << precacheModels
                  << Const(0) << precacheSprites;

        devPopup->addSeparatorLabel("Diagnosis");
        *devPopup << LabelWidget::newWithText("Surface Texturing:")
                  << rendTex
                  << LabelWidget::newWithText("Draw as Wireframe:")
                  << wireframe
                  << LabelWidget::newWithText("Bounds:")
                  << new CVarToggleWidget("rend-dev-mobj-bbox", "Mobj Bounding Boxes")
                  << Const(0)
                  << new CVarToggleWidget("rend-dev-polyobj-bbox", "Polyobj Bounding Boxes")
                  << LabelWidget::newWithText("Identifiers:")
                  << new CVarToggleWidget("rend-dev-thinker-ids", "Thinker IDs")
                  << Const(0)
                  << new CVarToggleWidget("rend-dev-sector-show-indices", "Sector Indices")
                  << Const(0)
                  << new CVarToggleWidget("rend-dev-vertex-show-indices", "Vertex Indices")
                  << Const(0)
                  << new CVarToggleWidget("rend-dev-generator-show-indices", "Particle Generator Indices");

        devPopup->commit();
    }

    void fetch()
    {
        for (GuiWidget *child : self().area().childWidgets() + devPopup->content().childWidgets())
        {
            if (ICVarWidget *w = maybeAs<ICVarWidget>(child))
            {
                w->updateFromCVar();
            }
        }
    }

    void apply()
    {
        if (texSettingsToggled)
        {
            GL_TexReset();
        }
    }
};

RendererSettingsDialog::RendererSettingsDialog(const String &name)
    : DialogWidget(name, WithHeading), d(new Impl(this))
{
    heading().setText("Renderer Settings");
    heading().setStyleImage("renderer");

    LabelWidget *appearLabel = LabelWidget::newWithText("Appearance:", &area());
    appearLabel->setName("appearance-label"); // for lookup from tutorial
    LabelWidget *fovLabel = LabelWidget::newWithText("Field of View:", &area());

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

        layout << *LabelWidget::newWithText("Pixel Density:", &area()) << *pd;
    }

    // Textures options.
    LabelWidget::appendSeparatorWithText("Textures", &area(), &layout);

    layout << *LabelWidget::newWithText("External Images:", &area()) << *d->enableExtWithPWADs
           << Const(0) << *d->disableExtTextures
           << Const(0) << *d->disableExtPatches;

    area().setContentSize(layout);

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, "Close")
            << new DialogButtonItem(DialogWidget::Action, "Reset to Defaults",
                                    [this]() { resetToDefaults(); })
            << new DialogButtonItem(DialogWidget::ActionPopup | Id1,
                                    style().images().image("gauge"));

    // Identifiers popup opens from the button.
    popupButtonWidget(Id1)->setPopup(*d->devPopup);

    audienceForClose() += [this]() { d->devPopup->close(); };
    d->appear->audienceForEditorRequest() += [this]() { editProfile(); };;

    d->fetch();

    auto toggledFunc = [this]() { d->texSettingsToggled = true; };

    d->enableExtWithPWADs->audienceForUserToggle() += toggledFunc;
    d->disableExtTextures->audienceForUserToggle() += toggledFunc;
    d->disableExtPatches->audienceForUserToggle()  += toggledFunc;
}

void RendererSettingsDialog::resetToDefaults()
{
    ClientApp::render().settings().resetToDefaults();

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

void RendererSettingsDialog::finish(int result)
{
    DialogWidget::finish(result);

    d->apply();
}
