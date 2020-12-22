/** @file openconnectiondialog.cpp  Dialog for specifying address for opening a connection.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/term/lineeditwidget.h>
#include <de/config.h>

using namespace de;

OpenConnectionDialog::OpenConnectionDialog(const String &name) : term::InputDialogWidget(name)
{
    setDescription("Enter the address of the server you want to connect to. "
                   "The address can be a domain name or an IP address. "
                   "Optionally, you may include a TCP port number, for example "
                   "\"10.0.1.1:13209\".");

    setPrompt("Address: ");
    lineEdit().setSignalOnEnter(false); // let menu handle it
    lineEdit().setText(Config::get().gets("OpenConnection.address", ""));

    setAcceptLabel("Connect to server");
}

String OpenConnectionDialog::address() const
{
    return text();
}

void OpenConnectionDialog::finish(int result)
{
    InputDialogWidget::finish(result);

    if (result)
    {
        Config::get().set("OpenConnection.address", text());
    }
}
