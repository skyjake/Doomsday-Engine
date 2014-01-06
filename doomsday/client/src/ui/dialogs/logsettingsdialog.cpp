/** @file logsettingsdialog.cpp  Dialog for Log settings.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/dialogs/logsettingsdialog.h"
#include "SignalAction"

using namespace de;

DENG2_PIMPL(LogSettingsDialog)
{
    Instance(Public *i) : Base(i)
    {}
};

LogSettingsDialog::LogSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Log Filter & Alerts"));

    /*
    LabelWidget *modeLabel     = LabelWidget::newWithText(tr("Stereo Mode:"), &area());
    LabelWidget *heightLabel   = LabelWidget::newWithText(tr("Height (m):"), &area());
    LabelWidget *ipdLabel      = LabelWidget::newWithText(tr("IPD (mm):"), &area());
    LabelWidget *dominantLabel = LabelWidget::newWithText(tr("Dominant Eye:"), &area());
    */

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);

    /*
    layout << *modeLabel     << *d->mode
           << *heightLabel   << *d->humanHeight
           << *ipdLabel      << *d->ipd
           << *dominantLabel << *d->dominantEye
           << Const(0)       << *d->swapEyes;
           */

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));
}

void LogSettingsDialog::resetToDefaults()
{
}
