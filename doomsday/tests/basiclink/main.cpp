/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <de/types.h>
#include "../testapp.h"
#include "clientserver.h"

#include <QObject>
#include <QDebug>

using namespace de;

int main(int argc, char** argv)
{
    try
    {
        TestApp app(argc, argv);
        
        if(app.commandLine().has("--server"))
        {
            qDebug("Running as server.");

            LOG_MESSAGE("Waiting for incoming connections...");

            Server server;
            app.mainLoop();
        }
        else
        {
            qDebug("Running as client.");

            LOG_MESSAGE("Waiting for data...");

            Client client(Address(QHostAddress::LocalHost, SERVER_PORT));
            app.mainLoop();
        }
    }
    catch(const Error& err)
    {
        qWarning() << err.asText();
    }

    qDebug("Exiting main()...");
    return 0;        
}
