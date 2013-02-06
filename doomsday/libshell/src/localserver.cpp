/** @file localserver.cpp  Starting and stopping local servers.
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

#include "de/shell/LocalServer"
#include "de/shell/Link"
#include <de/CommandLine>
#include <QCoreApplication>
#include <QDir>

namespace de {
namespace shell {

struct LocalServer::Instance
{
    Link *link;

    Instance() : link(0) {}
};

LocalServer::LocalServer() : d(new Instance)
{}

LocalServer::~LocalServer()
{
    delete d;
}

void LocalServer::start(duint16 port, String const &gameMode)
{
    DENG2_ASSERT(d->link == 0);

    CommandLine cmd;

#ifdef MACOSX
    /**
     * @todo These options will be much simpler when libdeng2 FS is used for
     * all file access.
     */
    cmd.append(String(qApp->applicationDirPath()) / "../Resources/doomsday-server");
    cmd.append("-userdir");
    cmd.append(QDir::home().filePath("Library/Application Support/Doomsday Engine/server-runtime"));
    cmd.append("-appdir");
    cmd.append(".");
    cmd.append("-vdmap");
    cmd.append("..");
    cmd.append("}Data");
    cmd.append("-basedir");
    cmd.append(String(qApp->applicationDirPath()) / "../Resources");
    String plugDir = String(qApp->applicationDirPath()) / "../DengPlugins";
    cmd.append("-vdmap");
    cmd.append(plugDir / "doom.bundle/Contents/Resources");
    cmd.append("}Data/jDoom/");
    cmd.append("-vdmap");
    cmd.append(plugDir / "heretic.bundle/Contents/Resources");
    cmd.append("}Data/jHeretic/");
    cmd.append("-vdmap");
    cmd.append(plugDir / "hexen.bundle/Contents/Resources");
    cmd.append("}Data/jHexen/");
#endif

    cmd.append("-game");
    cmd.append(gameMode);
    cmd.append("-cmd");
    cmd.append("net-ip-port " + String::number(port));

    LOG_INFO("Starting local server with port %i using game mode '%s'")
            << port << gameMode;

    cmd.execute();
}

void LocalServer::stop()
{
    DENG2_ASSERT(d->link != 0);
}

/*Link &LocalServer::link()
{
    DENG2_ASSERT(d->link != 0);

    return *d->link;
}*/

} // namespace shell
} // namespace de
