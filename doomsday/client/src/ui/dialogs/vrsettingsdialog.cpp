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
    //CVarToggleWidget *devInfo;
    CVarChoiceWidget *mode;

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
                << new ChoiceItem("Side by side", VR::MODE_SIDE_BY_SIDE)
                << new ChoiceItem("Parallel", VR::MODE_PARALLEL)
                << new ChoiceItem("Cross-eye", VR::MODE_CROSSEYE)
                << new ChoiceItem("Oculus Rift", VR::MODE_OCULUS_RIFT)
                << new ChoiceItem("Hardware stereo", VR::MODE_QUAD_BUFFERED);

            /* MODE_MONO = 0,
               MODE_GREEN_MAGENTA,
               MODE_RED_CYAN,
               MODE_LEFT,
               MODE_RIGHT,
               MODE_TOP_BOTTOM, // 5
               MODE_SIDE_BY_SIDE,
               MODE_PARALLEL,
               MODE_CROSSEYE,
               MODE_OCULUS_RIFT,
               MODE_ROW_INTERLEAVED, // 10 // NOT IMPLEMENTED YET
               MODE_COLUMN_INTERLEAVED, // NOT IMPLEMENTED YET
               MODE_CHECKERBOARD, // NOT IMPLEMENTED YET
               MODE_QUAD_BUFFERED,*/
        //area.add(devInfo = new CVarToggleWidget("net-dev"));
    }

    void fetch()
    {
        //devInfo->updateFromCVar();
        mode->updateFromCVar();
    }
};

VRSettingsDialog::VRSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("VR Settings"));

    //d->devInfo->setText(tr("Developer Info"));

    LabelWidget *modeLabel = LabelWidget::newWithText(tr("Mode:"), &area());

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    //layout.setColumnAlignment(0, ui::AlignRight);

    layout << *modeLabel << *d->mode;

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));

    d->fetch();
}

void VRSettingsDialog::resetToDefaults()
{
    Con_SetInteger("rend-vr-mode", VR::MODE_MONO);

    d->fetch();
}

