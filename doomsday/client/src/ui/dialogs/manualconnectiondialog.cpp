/** @file manualconnectiondialog.cpp  Dialog for connecting to a server.
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

#include "ui/dialogs/manualconnectiondialog.h"
#include <de/PersistentState>

using namespace de;

DENG2_PIMPL_NOREF(ManualConnectionDialog)
{
    String usedAddress;
};

ManualConnectionDialog::ManualConnectionDialog(String const &name)
    : InputDialog(name), d(new Instance)
{
    title().setText(tr("Connect to Server"));
    message().setText(tr("Enter the address of the multiplayer server you want to connect to. "
                         "The address can be a domain name or an IP address. "
                         "Optionally, you may include a TCP port number, for example "
                         _E(b) "10.0.1.1:13209" _E(.) "."));

    defaultActionItem()->setLabel(tr("Connect"));
    buttonWidget(tr("Connect")).disable();

    connect(&editor(), SIGNAL(editorContentChanged()), this, SLOT(validate()));
}

void ManualConnectionDialog::operator >> (PersistentState &toState) const
{
    toState.names().set(name() + ".address", d->usedAddress);
}

void ManualConnectionDialog::operator << (PersistentState const &fromState)
{
    d->usedAddress = fromState.names()[name() + ".address"];
    editor().setText(d->usedAddress);
    validate();
}

void ManualConnectionDialog::validate()
{
    bool valid = true;

    if(editor().text().isEmpty() || editor().text().contains(';') ||
       editor().text().endsWith(":") || editor().text().startsWith(":"))
    {
        valid = false;
    }

    buttonWidget(tr("Connect")).enable(valid);
}

void ManualConnectionDialog::finish(int result)
{
    if(result)
    {
        // The dialog was accepted.
        d->usedAddress = editor().text();
    }

    InputDialog::finish(result);
}

