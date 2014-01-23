/** @file multiplayerdialog.cpp  Dialog for listing found servers and joining games.
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

#include "ui/dialogs/multiplayerdialog.h"
#include <de/SignalAction>

using namespace de;

DENG2_PIMPL(MultiplayerDialog)
{
    Instance(Public *i) : Base(i)
    {

    }
};

MultiplayerDialog::MultiplayerDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Multiplayer"));

    LabelWidget *lab = LabelWidget::newWithText(tr("Games from Master Server and local network:"), &area());

    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(1, 0);
    //layout.setColumnAlignment(0, ui::AlignRight);

    layout << *lab;

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, style().images().image("gear"),
                                    new SignalAction(this, SLOT(showSettings())));
}

void MultiplayerDialog::showSettings()
{

}
