/** @file coloradjustmentdialog.cpp  Dialog for gamma, etc. adjustments.
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

#include "ui/dialogs/coloradjustmentdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include <de/gridlayout.h>
#include "api_console.h"

using namespace de;
using namespace de::ui;

DE_GUI_PIMPL(ColorAdjustmentDialog)
{
    CVarSliderWidget *gamma;
    CVarSliderWidget *contrast;
    CVarSliderWidget *brightness;

    Impl(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self().area();

        LabelWidget *gammaLabel      = LabelWidget::newWithText("Gamma:", &area);
        LabelWidget *contrastLabel   = LabelWidget::newWithText("Contrast:", &area);
        LabelWidget *brightnessLabel = LabelWidget::newWithText("Brightness:", &area);

        area.add(gamma = new CVarSliderWidget("vid-gamma"));
        area.add(contrast = new CVarSliderWidget("vid-contrast"));
        area.add(brightness = new CVarSliderWidget("vid-bright"));

        const Rule &sliderWidth = rule("coloradjustment.slider");
        gamma->rule().setInput(Rule::Width, sliderWidth);
        contrast->rule().setInput(Rule::Width, sliderWidth);
        brightness->rule().setInput(Rule::Width, sliderWidth);

        GridLayout layout(area.contentRule().left(), area.contentRule().top());
        layout.setGridSize(2, 3);
        layout.setColumnAlignment(0, ui::AlignRight);
        layout << *gammaLabel      << *gamma
               << *contrastLabel   << *contrast
               << *brightnessLabel << *brightness;
        auto *msg = LabelWidget::newWithText("These only affect the 3D game view.", &area);
        {
            msg->margins().setTop("unit").setBottom("");
            msg->setFont("separator.annotation");
            msg->setTextColor("altaccent");
        }
        layout << Const(0) << *msg;
        area.setContentSize(layout);
    }

    void fetch()
    {
        gamma->updateFromCVar();
        contrast->updateFromCVar();
        brightness->updateFromCVar();
    }
};

ColorAdjustmentDialog::ColorAdjustmentDialog(const String &name)
    : DialogWidget(name, WithHeading), d(new Impl(this))
{
    heading().setText("Color Adjustments");
    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, "Close")
            << new DialogButtonItem(DialogWidget::Action, "Reset to Defaults",
                                    [this](){ resetToDefaults(); });
}

void ColorAdjustmentDialog::prepare()
{
    DialogWidget::prepare();

    d->fetch();
}

void ColorAdjustmentDialog::resetToDefaults()
{
    Con_SetFloat("vid-gamma", 1);
    Con_SetFloat("vid-contrast", 1);
    Con_SetFloat("vid-bright", 0);

    d->gamma->updateFromCVar();
    d->contrast->updateFromCVar();
    d->brightness->updateFromCVar();
}
