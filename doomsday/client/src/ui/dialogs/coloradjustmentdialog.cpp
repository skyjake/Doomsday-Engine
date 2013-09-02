/** @file coloradjustmentdialog.cpp  Dialog for gamma, etc. adjustments.
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

#include "ui/dialogs/coloradjustmentdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "GridLayout"
#include "SignalAction"
#include "con_main.h"

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(ColorAdjustmentDialog)
{
    CVarSliderWidget *gamma;
    CVarSliderWidget *contrast;
    CVarSliderWidget *brightness;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        LabelWidget *gammaLabel = new LabelWidget;
        gammaLabel->setText(tr("Gamma:"));

        LabelWidget *contrastLabel = new LabelWidget;
        contrastLabel->setText(tr("Contrast:"));

        LabelWidget *brightnessLabel = new LabelWidget;
        brightnessLabel->setText(tr("Brightness:"));

        area.add(gammaLabel);
        area.add(gamma = new CVarSliderWidget("vid-gamma"));
        area.add(contrastLabel);
        area.add(contrast = new CVarSliderWidget("vid-contrast"));
        area.add(brightnessLabel);
        area.add(brightness = new CVarSliderWidget("vid-bright"));

        Rule const &sliderWidth = style().rules().rule("coloradjustment.slider");
        gamma->rule().setInput(Rule::Width, sliderWidth);
        contrast->rule().setInput(Rule::Width, sliderWidth);
        brightness->rule().setInput(Rule::Width, sliderWidth);

        GridLayout layout(area.contentRule().left(), area.contentRule().top());
        layout.setGridSize(2, 3);
        layout.setColumnAlignment(0, ui::AlignRight);
        layout << *gammaLabel      << *gamma
               << *contrastLabel   << *contrast
               << *brightnessLabel << *brightness;
        area.setContentSize(layout.width(), layout.height());
    }

    void fetch()
    {
        gamma->updateFromCVar();
        contrast->updateFromCVar();
        brightness->updateFromCVar();
    }
};

ColorAdjustmentDialog::ColorAdjustmentDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Color Adjustments"));
    buttons().items()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));
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
