/** @file openconnectiondialog.cpp  Dialog for specifying address for opening a connection.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "openconnectiondialog.h"
#include "persistentdata.h"
#include <de/shell/LineEditWidget>

using namespace de;

OpenConnectionDialog::OpenConnectionDialog(String const &name) : shell::InputDialog(name)
{
    setDescription(tr("Enter the address of the server you want to connect to. "
                      "The address can be a domain name or an IP address. "
                      "Optionally, you may include a TCP port number, for example "
                      "\"10.0.1.1:13209\"."));

    setPrompt(tr("Address: "));
    lineEdit().setSignalOnEnter(false); // let menu handle it
    lineEdit().setText(PersistentData::get("OpenConnection/address"));

    setAcceptLabel(tr("Connect to server"));
}

String OpenConnectionDialog::address()
{
    return text();
}

void OpenConnectionDialog::finish(int result)
{
    InputDialog::finish(result);

    if(result)
    {
        PersistentData::set("OpenConnection/address", text());
    }
}
