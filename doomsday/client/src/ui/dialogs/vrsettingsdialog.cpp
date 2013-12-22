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

    Instance(Public *i) : Base(i)
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

        area.add(riftPredictionLatency = new CVarSliderWidget("rend-vr-rift-latency"));
        riftPredictionLatency->setDisplayFactor(1000);
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
    LabelWidget *dominantLabel = LabelWidget::newWithText(tr("Dominant Eye:"), &area());
    LabelWidget *heightLabel   = LabelWidget::newWithText(tr("Height (m):"), &area());
    LabelWidget *ipdLabel      = LabelWidget::newWithText(tr("IPD (mm):"), &area());
    LabelWidget *latencyLabel  = LabelWidget::newWithText(tr("Prediction Latency:"), &area());

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);

    layout << *modeLabel     << *d->mode
           << Const(0)       << *d->swapEyes
           << *dominantLabel << *d->dominantEye
           << *heightLabel   << *d->humanHeight
           << *ipdLabel      << *d->ipd
           << *latencyLabel  << *d->riftPredictionLatency;

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

