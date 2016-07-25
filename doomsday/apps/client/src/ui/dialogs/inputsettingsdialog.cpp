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
#include "ui/widgets/keygrabberwidget.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/controllerpresets.h"
#include "clientapp.h"
#include "api_console.h"

#include <de/VariableToggleWidget>
#include <de/ChoiceWidget>
#include <de/GridPopupWidget>
#include <de/SignalAction>

using namespace de;
using namespace de::ui;

DENG_GUI_PIMPL(InputSettingsDialog)
{
    ChoiceWidget *gamepad;
    ButtonWidget *applyGamepad;
    VariableToggleWidget *syncMouse;
    CVarToggleWidget *syncInput;
    CVarSliderWidget *mouseSensiX;
    CVarSliderWidget *mouseSensiY;
    ToggleWidget *mouseDisableX;
    ToggleWidget *mouseDisableY;
    ToggleWidget *mouseInvertX;
    ToggleWidget *mouseInvertY;
    ToggleWidget *mouseFilterX;
    ToggleWidget *mouseFilterY;
    CVarToggleWidget *joyEnable;
    GridPopupWidget *devPopup;

    Impl(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        area.add(syncMouse     = new VariableToggleWidget(App::config("input.mouse.syncSensitivity")));
        area.add(mouseSensiX   = new CVarSliderWidget("input-mouse-x-scale"));
        area.add(mouseSensiY   = new CVarSliderWidget("input-mouse-y-scale"));
        area.add(mouseDisableX = new ToggleWidget);
        area.add(mouseDisableY = new ToggleWidget);
        area.add(mouseInvertX  = new ToggleWidget);
        area.add(mouseInvertY  = new ToggleWidget);
        area.add(mouseFilterX  = new ToggleWidget);
        area.add(mouseFilterY  = new ToggleWidget);

        // Gamepad.
        area.add(joyEnable     = new CVarToggleWidget("input-joy"));
        area.add(applyGamepad  = new ButtonWidget);
        area.add(gamepad       = new ChoiceWidget);

        gamepad->items() << new ChoiceItem(tr("None"), "");
        QStringList ids = ClientApp::inputSystem().gameControllerPresets().ids();
        ids.sort(Qt::CaseInsensitive);
        foreach (QString id, ids)
        {
            gamepad->items() << new ChoiceItem(id, id);
        }

        // Developer options.
        syncInput = new CVarToggleWidget("input-sharp");
        self.add(devPopup = new GridPopupWidget);
        devPopup->addSpanning(syncInput);
        *devPopup << LabelWidget::newWithText(tr("Key Grabber:"))
                  << new KeyGrabberWidget;
        devPopup->commit();
    }

    void fetch()
    {
        syncInput->updateFromCVar();
        mouseSensiX->updateFromCVar();
        mouseSensiY->updateFromCVar();
        joyEnable->updateFromCVar();

        mouseDisableX->setActive(Con_GetInteger("input-mouse-x-flags") & IDA_DISABLED);
        mouseDisableY->setActive(Con_GetInteger("input-mouse-y-flags") & IDA_DISABLED);
        mouseInvertX->setActive(Con_GetInteger("input-mouse-x-flags") & IDA_INVERT);
        mouseInvertY->setActive(Con_GetInteger("input-mouse-y-flags") & IDA_INVERT);
        mouseFilterX->setInactive(Con_GetInteger("input-mouse-x-flags") & IDA_RAW);
        mouseFilterY->setInactive(Con_GetInteger("input-mouse-y-flags") & IDA_RAW);

        enableOrDisable();

        gamepad->setSelected(gamepad->items().findData(
                                 ClientApp::inputSystem().gameControllerPresets().currentPreset()));
    }

    void enableOrDisable()
    {
        mouseSensiX->disable(mouseDisableX->isActive());
        mouseSensiY->disable(mouseDisableY->isActive());
        mouseInvertX->disable(mouseDisableX->isActive());
        mouseInvertY->disable(mouseDisableY->isActive());
        mouseFilterX->disable(mouseDisableX->isActive());
        mouseFilterY->disable(mouseDisableY->isActive());
    }

    void updateMouseFlags()
    {
        Con_SetInteger("input-mouse-x-flags",
                       (mouseDisableX->isActive()?  IDA_DISABLED : 0) |
                       (mouseInvertX->isActive()?   IDA_INVERT   : 0) |
                       (mouseFilterX->isInactive()? IDA_RAW      : 0));

        Con_SetInteger("input-mouse-y-flags",
                       (mouseDisableY->isActive()?  IDA_DISABLED : 0) |
                       (mouseInvertY->isActive()?   IDA_INVERT   : 0) |
                       (mouseFilterY->isInactive()? IDA_RAW      : 0));

        enableOrDisable();
    }
};

InputSettingsDialog::InputSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Impl(this))
{
    heading().setText(tr("Input Settings"));
    heading().setImage(style().images().image("input"));

    d->syncInput->setText(tr("Vanilla 35Hz Input Rate"));
    d->syncMouse->setText(tr("Sync Axis Sensitivities"));
    d->applyGamepad->setText(tr("Apply Preset"));
    connect(d->applyGamepad, SIGNAL(pressed()), this, SLOT(applyControllerPreset()));

    LabelWidget *mouseXLabel = LabelWidget::newWithText(_E(D) + tr("Mouse: Horizontal"), &area());
    LabelWidget *mouseYLabel = LabelWidget::newWithText(_E(D) + tr("Mouse: Vertical"), &area());
    mouseXLabel->setFont("separator.label");
    mouseYLabel->setFont("separator.label");

    mouseXLabel->margins().setTop("gap");
    mouseYLabel->margins().setTop("gap");

    LabelWidget *applyNote = LabelWidget::newWithText(tr("Clicking " _E(b) "Apply Preset" _E(.) " will remove all "
                                                         "existing game controller bindings and apply "
                                                         "the selected preset."), &area());
    applyNote->margins().setTop("");
    applyNote->setFont("separator.annotation");
    applyNote->setTextColor("altaccent");
    applyNote->setTextLineAlignment(ui::AlignLeft);

    // The sensitivity cvars are unlimited.
    d->mouseSensiX->setRange(Rangef(.00005f, .0075f));
    d->mouseSensiX->setDisplayFactor(1000);
    d->mouseSensiY->setRange(Rangef(.00005f, .0075f));
    d->mouseSensiY->setDisplayFactor(1000);

    connect(d->mouseSensiX, SIGNAL(valueChangedByUser(double)), this, SLOT(mouseSensitivityChanged(double)));
    connect(d->mouseSensiY, SIGNAL(valueChangedByUser(double)), this, SLOT(mouseSensitivityChanged(double)));

    d->mouseInvertX->setText(tr("Invert X Axis"));
    d->mouseDisableX->setText(tr("Disable X Axis"));
    d->mouseFilterX->setText(tr("Filter X Axis"));

    d->mouseInvertY->setText(tr("Invert Y Axis"));
    d->mouseDisableY->setText(tr("Disable Y Axis"));
    d->mouseFilterY->setText(tr("Filter Y Axis"));

    connect(d->mouseInvertX, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));
    connect(d->mouseInvertY, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));
    connect(d->mouseDisableX, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));
    connect(d->mouseDisableY, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));
    connect(d->mouseFilterX, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));
    connect(d->mouseFilterY, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), this, SLOT(mouseTogglesChanged()));

    d->joyEnable->setText(tr("Game Controllers Enabled"));
    d->syncMouse->margins().setBottom("gap");

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    //layout.setColumnAlignment(0, ui::AlignRight);
    layout << *LabelWidget::newWithText(tr("Game Controller:"), &area()) << *d->gamepad;
    d->applyGamepad->setSizePolicy(ui::Expand, ui::Expand);
    d->applyGamepad->rule()
            .setInput(Rule::Left, d->gamepad->rule().right())
            .setMidAnchorY(d->gamepad->rule().midY());
    layout.append(*applyNote, 2);
    layout.append(*d->joyEnable, 2);

    GridLayout layout2(area().contentRule().left(), d->joyEnable->rule().bottom());
    layout2.setGridSize(2, 0);
    layout2 << *mouseXLabel      << *mouseYLabel
            << *d->mouseSensiX   << *d->mouseSensiY;
    layout2.append(*d->syncMouse, 2);
    layout2 << *d->mouseInvertX  << *d->mouseInvertY
            << *d->mouseFilterX  << *d->mouseFilterY
            << *d->mouseDisableX << *d->mouseDisableY;

    applyNote->setMaximumTextWidth(layout2.width() - rule("dialog.gap"));

    area().setContentSize(OperatorRule::maximum(layout.width(), layout2.width()),
                          layout.height() + layout2.height());

    buttons()
            << new DialogButtonItem(Default | Accept, tr("Close"))
            << new DialogButtonItem(Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())))
            << new DialogButtonItem(ActionPopup | Id1,
                                    style().images().image("gauge"));

    popupButtonWidget(Id1)->setPopup(*d->devPopup);

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
    if (d->syncMouse->isActive())
    {
        if (sender() == d->mouseSensiX)
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

void InputSettingsDialog::applyControllerPreset()
{
    String const presetId = d->gamepad->selectedItem().data().toString();
    ClientApp::inputSystem().gameControllerPresets().applyPreset(presetId);
}
