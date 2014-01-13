/** @file inputdialog.cpp Dialog for querying a string of text from the user.
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

#include "de/InputDialog"

namespace de {

DENG2_PIMPL_NOREF(InputDialog)
{
    LineEditWidget *editor;
};

InputDialog::InputDialog(String const &name)
    : MessageDialog(name), d(new Instance)
{
    // Create the editor.
    area().add(d->editor = new LineEditWidget);
    d->editor->setSignalOnEnter(true);
    connect(d->editor, SIGNAL(enterPressed(QString)), this, SLOT(accept()));

    buttons()
            << new DialogButtonItem(Default | Accept)
            << new DialogButtonItem(Reject);

    updateLayout();
}

LineEditWidget &InputDialog::editor()
{
    return *d->editor;
}

void InputDialog::preparePanelForOpening()
{
    MessageDialog::preparePanelForOpening();
    root().setFocus(d->editor);
}

void InputDialog::panelClosing()
{
    MessageDialog::panelClosing();
    root().setFocus(0);
}

} // namespace de
