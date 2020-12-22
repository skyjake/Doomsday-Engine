/** @file inputsettingsdialog.cpp Dialog for input settings.
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

#include "ui/dialogs/inputsettingsdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/keygrabberwidget.h"
#include "ui/axisinputcontrol.h"
#include "ui/controllerpresets.h"
#include "ui/inputsystem.h"
#include "clientapp.h"
#include "api_console.h"

#include <de/variabletogglewidget.h>
#include <de/choicewidget.h>
#include <de/gridpopupwidget.h>
#include <de/textvalue.h>

using namespace de;
using namespace de::ui;

DE_GUI_PIMPL(InputSettingsDialog)
, DE_OBSERVES(SliderWidget, UserValue)
{
    ChoiceWidget *        gamepad;
    ButtonWidget *        applyGamepad;
    VariableToggleWidget *syncMouse;
    CVarToggleWidget *    syncInput;
    CVarSliderWidget *    mouseSensiX;
    CVarSliderWidget *    mouseSensiY;
    ToggleWidget *        mouseDisableX;
    ToggleWidget *        mouseDisableY;
    ToggleWidget *        mouseInvertX;
    ToggleWidget *        mouseInvertY;
    ToggleWidget *        mouseFilterX;
    ToggleWidget *        mouseFilterY;
    CVarToggleWidget *    joyEnable;
    GridPopupWidget *     devPopup;

    Impl(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self().area();

        // Gamepad.
        area.add(joyEnable     = new CVarToggleWidget("input-joy"));
        area.add(gamepad       = new ChoiceWidget);
        area.add(applyGamepad  = new ButtonWidget);

        area.add(syncMouse     = new VariableToggleWidget(App::config("input.mouse.syncSensitivity")));

        area.add(mouseSensiX   = new CVarSliderWidget("input-mouse-x-factor"));
        area.add(mouseInvertX  = new ToggleWidget);
        area.add(mouseFilterX  = new ToggleWidget);
        area.add(mouseDisableX = new ToggleWidget);

        area.add(mouseSensiY   = new CVarSliderWidget("input-mouse-y-factor"));
        area.add(mouseInvertY  = new ToggleWidget);
        area.add(mouseFilterY  = new ToggleWidget);
        area.add(mouseDisableY = new ToggleWidget);

        gamepad->items() << new ChoiceItem("None", TextValue());
        StringList ids = ClientApp::input().gameControllerPresets().ids();
        std::sort(ids.begin(), ids.end(), [](const String &a, const String &b) {
            return a.compareWithoutCase(b) < 0; });
        for (const String &id : ids)
        {
            gamepad->items() << new ChoiceItem(id, TextValue(id));
        }

        // Developer options.
        syncInput = new CVarToggleWidget("input-sharp");
        self().add(devPopup = new GridPopupWidget);
        devPopup->addSpanning(syncInput);
        *devPopup << LabelWidget::newWithText("Key Grabber:")
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
            TextValue(ClientApp::input().gameControllerPresets().currentPreset())));
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

    void sliderValueChangedByUser(SliderWidget &slider, double value) override
    {
        // Keep mouse axes synced?
        if (syncMouse->isActive())
        {
            if (&slider == mouseSensiX)
            {
                mouseSensiY->setValue(value);
                mouseSensiY->setCVarValueFromWidget();
    }
            else
            {
                mouseSensiX->setValue(value);
                mouseSensiX->setCVarValueFromWidget();
            }
        }
    }
};

InputSettingsDialog::InputSettingsDialog(const String &name)
    : DialogWidget(name, WithHeading), d(new Impl(this))
{
    heading().setText("Input Settings");
    heading().setImage(style().images().image("input"));

    d->syncInput->setText("Vanilla 35Hz Input Rate");
    d->syncMouse->setText("Sync Axis Sensitivities");
    d->applyGamepad->setText("Apply");
    d->applyGamepad->audienceForPress() += [this](){ applyControllerPreset(); };

    LabelWidget *mouseXLabel = LabelWidget::appendSeparatorWithText("Mouse: Horizontal", &area());
    LabelWidget *mouseYLabel = LabelWidget::appendSeparatorWithText("Mouse: Vertical", &area());

    LabelWidget *applyNote = LabelWidget::newWithText("Clicking " _E(b) "Apply" _E(.) " will remove all "
                                                         "existing game controller bindings and apply "
                                                      "the selected preset.", &area());
    applyNote->margins().setTop("");
    applyNote->setFont("separator.annotation");
    applyNote->setTextColor("altaccent");
    applyNote->setTextLineAlignment(ui::AlignLeft);

    // The sensitivity cvars are unlimited.
    d->mouseSensiX->setRange(Rangef(.5f, 75.f));
    d->mouseSensiX->setDisplayFactor(0.1);
    d->mouseSensiY->setRange(Rangef(.5f, 75.f));
    d->mouseSensiY->setDisplayFactor(0.1);

    d->mouseSensiX->audienceForUserValue() += d;
    d->mouseSensiY->audienceForUserValue() += d;

    d->mouseInvertX->setText ("Invert X Axis");
    d->mouseDisableX->setText("Disable X Axis");
    d->mouseFilterX->setText ("Filter X Axis");

    d->mouseInvertY->setText ("Invert Y Axis");
    d->mouseDisableY->setText("Disable Y Axis");
    d->mouseFilterY->setText ("Filter Y Axis");

    for (auto *w : {d->mouseInvertX, d->mouseInvertY, d->mouseDisableX, d->mouseDisableY, d->mouseFilterX, d->mouseFilterY})
    {
        w->audienceForUserToggle() += [this](){ mouseTogglesChanged(); };
    }

    d->joyEnable->setText("Game Controllers Enabled");
    d->syncMouse->margins().setBottom("gap");

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    //layout.setColumnAlignment(0, ui::AlignRight);
    layout.append(*d->joyEnable, 2);
    layout << *LabelWidget::newWithText("Game Controller Preset:", &area())
           << *d->gamepad;
    d->applyGamepad->setSizePolicy(ui::Expand, ui::Expand);
    d->applyGamepad->rule()
            .setInput(Rule::Left, d->gamepad->rule().right())
            .setMidAnchorY(d->gamepad->rule().midY());
    layout.append(*applyNote, 2);

    GridLayout layout2(area().contentRule().left(), applyNote->rule().bottom());
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
            << new DialogButtonItem(Default | Accept, "Close")
            << new DialogButtonItem(Action, "Reset to Defaults",
                                    [this]() { resetToDefaults(); })
            << new DialogButtonItem(ActionPopup | Id1,
                                    style().images().image("gauge"));

    popupButtonWidget(Id1)->setPopup(*d->devPopup);

    d->fetch();
}

void InputSettingsDialog::resetToDefaults()
{
    ClientApp::input().settings().resetToDefaults();

    d->fetch();
}

void InputSettingsDialog::mouseTogglesChanged()
{
    d->updateMouseFlags();
}

void InputSettingsDialog::applyControllerPreset()
{
    const String presetId = d->gamepad->selectedItem().data().asText();
    ClientApp::input().gameControllerPresets().applyPreset(presetId);
}
