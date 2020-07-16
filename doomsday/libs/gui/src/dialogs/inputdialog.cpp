/** @file dialogs/inputdialog.cpp Dialog for querying a string of text from the user.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/inputdialog.h"

namespace de {

DE_PIMPL_NOREF(InputDialog)
{
    LineEditWidget *editor;
};

InputDialog::InputDialog(const String &name)
    : MessageDialog(name), d(new Impl)
{
    // Create the editor.
    area().add(d->editor = new LineEditWidget);
    d->editor->setSignalOnEnter(true);
    d->editor->audienceForEnter() += [this](){ accept(); };

    buttons() << new DialogButtonItem(Default | Accept)
              << new DialogButtonItem(Reject);

    updateLayout();
}

LineEditWidget &InputDialog::editor()
{
    return *d->editor;
}

const LineEditWidget &InputDialog::editor() const
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
    root().setFocus(nullptr);
}

} // namespace de
