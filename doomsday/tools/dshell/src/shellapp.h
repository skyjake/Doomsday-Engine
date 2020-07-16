/** @file shellapp.h Doomsday shell connection app.
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

#ifndef SHELLAPP_H
#define SHELLAPP_H

#include "cursesapp.h"
#include <de/address.h>

class ShellApp : public CursesApp
{
public:
    ShellApp(int &argc, char **argv);

    ~ShellApp();

    void openConnection(const de::String &address);

    void showAbout();
    void askToOpenConnection();
    void askToStartLocalServer();
    void updateMenuWithFoundServers();
    void connectToFoundServer();
    void closeConnection();
    void askForPassword();
    void sendCommandToServer(const de::String& command);
    void handleIncomingPackets();
    void disconnected();
    void openMenu();
    void menuClosed();

private:
    DE_PRIVATE(d)
};

#endif // SHELLAPP_H
