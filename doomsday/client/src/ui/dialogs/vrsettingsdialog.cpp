/** @file vrsettingsdialog.cpp  Settings for virtual reality.
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

#include "ui/dialogs/vrsettingsdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/cvarchoicewidget.h"

#include "render/vr.h"
#include "con_main.h"
#include "SignalAction"

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(VRSettingsDialog)
{
    CVarChoiceWidget *mode;
    CVarToggleWidget *swapEyes;
    CVarSliderWidget *dominantEye;
    CVarSliderWidget *humanHeight;
    CVarSliderWidget *ipd;
    CVarSliderWidget *riftPredictionLatency;
    ButtonWidget *riftSetup;
    ButtonWidget *desktopSetup;

    Instance(Public *i)
        : Base(i)
        , riftPredictionLatency(0)
        , riftSetup(0)
    {
        ScrollAreaWidget &area = self.area();

        area.add(mode = new CVarChoiceWidget("rend-vr-mode"));
        mode->items()
                << new ChoiceItem("Mono", VR::MODE_MONO)
                << new ChoiceItem("Anaglyph (green/magenta)", VR::MODE_GREEN_MAGENTA)
                << new ChoiceItem("Anaglyph (red/cyan)", VR::MODE_RED_CYAN)
                << new ChoiceItem("Left eye only", VR::MODE_LEFT)
                << new ChoiceItem("Right eye only", VR::MODE_RIGHT)
                << new ChoiceItem("Top/bottom", VR::MODE_TOP_BOTTOM)
                << new ChoiceItem("Side-by-side", VR::MODE_SIDE_BY_SIDE)
                << new ChoiceItem("Parallel", VR::MODE_PARALLEL)
                << new ChoiceItem("Cross-eye", VR::MODE_CROSSEYE)
                << new ChoiceItem("Oculus Rift", VR::MODE_OCULUS_RIFT)
                << new ChoiceItem("Hardware stereo", VR::MODE_QUAD_BUFFERED);

        area.add(swapEyes    = new CVarToggleWidget("rend-vr-swap-eyes", tr("Swap Eyes")));
        area.add(dominantEye = new CVarSliderWidget("rend-vr-dominant-eye"));
        area.add(humanHeight = new CVarSliderWidget("rend-vr-player-height"));

        area.add(ipd = new CVarSliderWidget("rend-vr-ipd"));
        ipd->setDisplayFactor(1000);

        if(VR::hasHeadOrientation())
        {
            area.add(riftPredictionLatency = new CVarSliderWidget("rend-vr-rift-latency"));
            riftPredictionLatency->setDisplayFactor(1000);

            area.add(riftSetup = new ButtonWidget);
            riftSetup->setText(tr("Apply Rift Settings"));
            riftSetup->setAction(new SignalAction(thisPublic, SLOT(autoConfigForOculusRift())));

            area.add(desktopSetup = new ButtonWidget);
            desktopSetup->setText(tr("Apply Desktop Settings"));
            desktopSetup->setAction(new SignalAction(thisPublic, SLOT(autoConfigForDesktop())));
        }
    }

    void fetch()
    {
        foreach(Widget *child, self.area().childWidgets())
        {
            if(ICVarWidget *w = child->maybeAs<ICVarWidget>())
            {
                w->updateFromCVar();
            }
        }
    }
};

VRSettingsDialog::VRSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("3D & VR Settings"));

    LabelWidget *modeLabel     = LabelWidget::newWithText(tr("Mode:"), &area());
    LabelWidget *heightLabel   = LabelWidget::newWithText(tr("Height (m):"), &area());
    LabelWidget *ipdLabel      = LabelWidget::newWithText(tr("IPD (mm):"), &area());
    LabelWidget *dominantLabel = LabelWidget::newWithText(tr("Dominant Eye:"), &area());

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);

    layout << *modeLabel     << *d->mode
           << *heightLabel   << *d->humanHeight
           << *ipdLabel      << *d->ipd
           << *dominantLabel << *d->dominantEye
           << Const(0)       << *d->swapEyes;

    if(VR::hasHeadOrientation())
    {
        LabelWidget *ovrLabel     = LabelWidget::newWithText(_E(1)_E(D) + tr("Oculus Rift"), &area());
        LabelWidget *latencyLabel = LabelWidget::newWithText(tr("Prediction Latency:"), &area());
        LabelWidget *utilLabel    = LabelWidget::newWithText(tr("Utilities:"), &area());

        ovrLabel->margins().setTop("gap");

        layout.setCellAlignment(Vector2i(0, 5), ui::AlignLeft);

        layout.append(*ovrLabel, 2);
        layout << *latencyLabel << *d->riftPredictionLatency
               << *utilLabel    << *d->riftSetup
               << Const(0)      << *d->desktopSetup;
    }

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));

    d->fetch();
}

void VRSettingsDialog::resetToDefaults()
{
    Con_SetInteger("rend-vr-mode",          VR::MODE_MONO);
    Con_SetInteger("rend-vr-swap-eyes",     0);
    Con_SetFloat  ("rend-vr-dominant-eye",  0);
    Con_SetFloat  ("rend-vr-player-height", 1.75f);
    Con_SetFloat  ("rend-vr-ipd",           0.064f);
    Con_SetFloat  ("rend-vr-rift-latency",  0.030f);

    d->fetch();
}

void VRSettingsDialog::autoConfigForOculusRift()
{
    Con_Execute(CMDS_DDAY, "setfullres 1280 800", false, false);
    Con_Execute(CMDS_DDAY, "bindcontrol lookpitch head-pitch", false, false);
    Con_Execute(CMDS_DDAY, "bindcontrol yawbody head-yaw", false, false);

    /// @todo This would be a good use case for cvar overriding. -jk

    Con_SetInteger("rend-vr-mode", VR::MODE_OCULUS_RIFT);
    Con_SetInteger("vid-fsaa", 0);
    Con_SetFloat  ("vid-gamma", 1.176f);
    Con_SetFloat  ("vid-contrast", 1.186f);
    Con_SetFloat  ("vid-bright", .034f);
    Con_SetFloat  ("view-bob-height", .2f);
    Con_SetFloat  ("msg-scale", 1);
    Con_SetFloat  ("hud-scale", 1);

    d->fetch();
}

void VRSettingsDialog::autoConfigForDesktop()
{
    Con_SetInteger("rend-vr-mode", VR::MODE_MONO);
    Con_SetFloat  ("vid-gamma", 1);
    Con_SetFloat  ("vid-contrast", 1);
    Con_SetFloat  ("vid-bright", 0);
    Con_SetFloat  ("view-bob-height", 1);
    Con_SetFloat  ("msg-scale", .8f);
    Con_SetFloat  ("hud-scale", .6f);

    d->fetch();
}
