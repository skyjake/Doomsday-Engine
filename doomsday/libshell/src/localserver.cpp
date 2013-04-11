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
#include "de/shell/DoomsdayInfo"
#include <de/CommandLine>
#include <QCoreApplication>
#include <QDir>

namespace de {
namespace shell {

DENG2_PIMPL_NOREF(LocalServer)
{
    Link *link;
    duint16 port;
    String name;

    Instance() : link(0), port(0) {}
};

LocalServer::LocalServer() : d(new Instance)
{}

void LocalServer::setName(String const &name)
{
    d->name = name;
    d->name.replace("\"", "\\\""); // for use on command line
}

void LocalServer::start(duint16 port, String const &gameMode, QStringList additionalOptions,
                        NativePath const &runtimePath)
{
    d->port = port;

    NativePath userDir = runtimePath;

    if(userDir.isEmpty())
    {
        // Default runtime location.
        userDir = DoomsdayInfo::defaultServerRuntimeFolder();
    }

    DENG2_ASSERT(d->link == 0);

    CommandLine cmd;

#ifdef MACOSX
    // First locate the server executable.
    NativePath bin = NativePath(qApp->applicationDirPath()) / "../Resources/doomsday-server";
    if(!bin.exists())
    {
        // Try another location: Doomsday-Shell.app -> Doomsday Engine.app/Contents/Doomsday.app
        bin = NativePath(qApp->applicationDirPath()) /
                "../../../Doomsday Engine.app/Contents/Doomsday.app/Contents/Resources/doomsday-server";
    }
    if(!bin.exists())
    {
        // Yet another possibility: Doomsday-Shell.app -> Doomsday.app
        bin = NativePath(qApp->applicationDirPath()) /
                "../../../Doomsday.app/Contents/Resources/doomsday-server";
    }
    if(!bin.exists())
    {
        // Undeployed, e.g., a developer build.
        bin = NativePath(qApp->applicationDirPath()) /
                "../../../../../../client/Doomsday.app/Contents/Resources/doomsday-server";
    }
    if(!bin.exists())
    {
        throw NotFoundError("LocalServer::start", "Could not find Doomsday.app");
    }

    /**
     * @todo These options will be much simpler when libdeng2 FS is used for
     * all file access.
     */

    cmd.append(bin);
    cmd.append("-vdmap");
    cmd.append("..");
    cmd.append("}Data");
    cmd.append("-basedir");
    cmd.append(bin.fileNamePath() / "../Resources");
    String plugDir = bin.fileNamePath() / "../DengPlugins";
    cmd.append("-vdmap");
    cmd.append(plugDir / "doom.bundle/Contents/Resources");
    cmd.append("}Data/jDoom/");
    cmd.append("-vdmap");
    cmd.append(plugDir / "heretic.bundle/Contents/Resources");
    cmd.append("}Data/jHeretic/");
    cmd.append("-vdmap");
    cmd.append(plugDir / "hexen.bundle/Contents/Resources");
    cmd.append("}Data/jHexen/");

#elif WIN32
    NativePath bin = NativePath(qApp->applicationDirPath()) / "doomsday-server.exe";
    cmd.append(bin);
    cmd.append("-basedir");
    cmd.append(bin.fileNamePath() / "..");

#else // UNIX
    NativePath bin = NativePath(qApp->applicationDirPath()) / "doomsday-server";
    if(!bin.exists()) bin = "doomsday-server"; // Perhaps it's on the path?
    cmd.append(bin);
#endif

    cmd.append("-userdir");
    cmd.append(userDir);
    cmd.append("-game");
    cmd.append(gameMode);
    cmd.append("-cmd");
    cmd.append("net-ip-port " + String::number(port));

    if(!d->name.isEmpty())
    {
        cmd.append("-cmd");
        cmd.append("server-name \"" + d->name + "\"");
    }

    foreach(String opt, additionalOptions) cmd.append(opt);

    LOG_INFO("Starting local server with port %i using game mode '%s'")
            << port << gameMode;

    cmd.execute();
}

void LocalServer::stop()
{
    DENG2_ASSERT(d->link != 0);
}

Link *LocalServer::openLink()
{
    return new Link(String("localhost:%1").arg(d->port), 30);
}

} // namespace shell
} // namespace de
