/** @file inputsettingsdialog.cpp Dialog for input settings.
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

#include "ui/dialogs/inputsettingsdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/variabletogglewidget.h"

#include "clientapp.h"
#include "con_main.h"
#include "SignalAction"

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(InputSettingsDialog)
{
    VariableToggleWidget *syncMouse;
    CVarSliderWidget *mouseSensiX;
    CVarSliderWidget *mouseSensiY;
    ToggleWidget *mouseDisableX;
    ToggleWidget *mouseDisableY;
    ToggleWidget *mouseInvertX;
    ToggleWidget *mouseInvertY;
    CVarToggleWidget *joyEnable;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        area.add(syncMouse     = new VariableToggleWidget(App::config()["input.mouse.syncSensitivity"]));
        area.add(mouseSensiX   = new CVarSliderWidget("input-mouse-x-scale"));
        area.add(mouseSensiY   = new CVarSliderWidget("input-mouse-y-scale"));
        area.add(mouseDisableX = new ToggleWidget);
        area.add(mouseDisableY = new ToggleWidget);
        area.add(mouseInvertX  = new ToggleWidget);
        area.add(mouseInvertY  = new ToggleWidget);
        area.add(joyEnable     = new CVarToggleWidget("input-joy"));
    }

    void fetch()
    {
        mouseSensiX->updateFromCVar();
        mouseSensiY->updateFromCVar();
        joyEnable->updateFromCVar();

        mouseDisableX->setActive(Con_GetInteger("input-mouse-x-flags") & IDA_DISABLED);
        mouseDisableY->setActive(Con_GetInteger("input-mouse-y-flags") & IDA_DISABLED);
        mouseInvertX->setActive(Con_GetInteger("input-mouse-x-flags") & IDA_INVERT);
        mouseInvertY->setActive(Con_GetInteger("input-mouse-y-flags") & IDA_INVERT);

        enableOrDisable();
    }

    void enableOrDisable()
    {
        mouseSensiX->disable(mouseDisableX->isActive());
        mouseSensiY->disable(mouseDisableY->isActive());
        mouseInvertX->disable(mouseDisableX->isActive());
        mouseInvertY->disable(mouseDisableY->isActive());
    }

    void updateMouseFlags()
    {
        Con_SetInteger("input-mouse-x-flags",
                       (mouseDisableX->isActive()? IDA_DISABLED : 0) |
                       (mouseInvertX->isActive()?  IDA_INVERT   : 0));

        Con_SetInteger("input-mouse-y-flags",
                       (mouseDisableY->isActive()? IDA_DISABLED : 0) |
                       (mouseInvertY->isActive()?  IDA_INVERT   : 0));

        enableOrDisable();
    }
};

InputSettingsDialog::InputSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Input Settings"));

    d->syncMouse->setText(tr("Uniform Mouse Axis Sensitivity"));

    LabelWidget *mouseXLabel = LabelWidget::newWithText(_E(1) + tr("Mouse X"), &area());
    LabelWidget *mouseYLabel = LabelWidget::newWithText(_E(1) + tr("Mouse Y"), &area());

    mouseXLabel->margins().setTop(style().rules().rule("gap"));
    mouseYLabel->margins().setTop(style().rules().rule("gap"));

    // The sensitivity cvars are unlimited.
    d->mouseSensiX->setRange(Rangef(.00005f, .0075f));
    d->mouseSensiX->setDisplayFactor(1000);
    d->mouseSensiY->setRange(Rangef(.00005f, .0075f));
    d->mouseSensiY->setDisplayFactor(1000);

    connect(d->mouseSensiX, SIGNAL(valueChangedByUser(double)), this, SLOT(mouseSensitivityChanged(double)));
    connect(d->mouseSensiY, SIGNAL(valueChangedByUser(double)), this, SLOT(mouseSensitivityChanged(double)));

    d->mouseInvertX->setText(tr("Invert Axis"));
    d->mouseDisableX->setText(tr("Disable Axis"));

    d->mouseInvertY->setText(tr("Invert Axis"));
    d->mouseDisableY->setText(tr("Disable Axis"));

    connect(d->mouseInvertX, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));
    connect(d->mouseInvertY, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));
    connect(d->mouseDisableX, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));
    connect(d->mouseDisableY, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));

    d->joyEnable->setText(tr("Joystick Enabled"));

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    //layout.setColumnAlignment(0, ui::AlignRight);
    layout.append(*d->joyEnable, 2)
          .append(*d->syncMouse, 2);
    layout << *mouseXLabel      << *mouseYLabel
           << *d->mouseSensiX   << *d->mouseSensiY
           << *d->mouseInvertX  << *d->mouseInvertY
           << *d->mouseDisableX << *d->mouseDisableY;

    area().setContentSize(layout.width(), layout.height());

    buttons().items()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));

    d->fetch();
}

void InputSettingsDialog::resetToDefaults()
{
    ClientApp::inputSystem().settings().resetToDefaults();

    d->fetch();
}

void InputSettingsDialog::mouseTogglesChanged()
{
    d->updateMouseFlags();
}

void InputSettingsDialog::mouseSensitivityChanged(double value)
{
    // Keep mouse axes synced?
    if(d->syncMouse->isActive())
    {
        if(sender() == d->mouseSensiX)
        {
            d->mouseSensiY->setValue(value);
            d->mouseSensiY->setCVarValueFromWidget();
        }
        else
        {
            d->mouseSensiX->setValue(value);
            d->mouseSensiX->setCVarValueFromWidget();
        }
    }
}
