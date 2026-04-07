/** @file localserver.cpp  Starting and stopping local servers.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/network/localserver.h"
#include "doomsday/network/link.h"
#include "doomsday/doomsdayinfo.h"
#include <de/commandline.h>
#include <de/nativepath.h>
#include <de/app.h>

namespace network {

static const char *ERROR_LOG_NAME = "doomsday-errors.out";

DE_PIMPL_NOREF(LocalServer)
{
//    Link *     link = nullptr;
    NativePath appPath; // where to find the server
    duint16    port = 0;
    String     name;
    NativePath userDir;

    tF::ref<iProcess> proc;
};

LocalServer::LocalServer() : d(new Impl)
{}

void LocalServer::setName(const String &name)
{
    d->name = name;
    d->name.replace("\"", "\\\""); // for use on command line
}

void LocalServer::setApplicationPath(const NativePath &path)
{
    d->appPath = path;
}

void LocalServer::start(duint16 port,
                        const String &gameMode,
                        const StringList &additionalOptions,
                        const NativePath &runtimePath)
{
    d->port    = port;
    d->userDir = runtimePath;

    if (d->userDir.isEmpty())
    {
        // Default runtime location.
        d->userDir = DoomsdayInfo::defaultServerRuntimeFolder();
    }

    // Get rid of a previous error log in this location.
    (d->userDir / ERROR_LOG_NAME).destroy();

//    DE_ASSERT(d->link == nullptr);

    CommandLine cmd;
    NativePath bin;

#ifdef MACOSX
    // First locate the server executable.
    if (!d->appPath.isEmpty())
    {
        bin = d->appPath / "Doomsday.app/Contents/MacOS/doomsday-server";
        if (!bin.exists())
        {
            bin = d->appPath / "Contents/MacOS/doomsday-server";
        }
    }
    if (!bin.exists())
    {
        bin = App::executableDir() / "../MacOS/doomsday-server";
    }
    if (!bin.exists())
    {
        // Yet another possibility: Doomsday Shell.app -> Doomsday.app
        // App folder randomization means this is only useful in developer builds, though.
        bin = App::executableDir() /
                "../../../Doomsday.app/Contents/MacOS/doomsday-server";
    }
    if (!bin.exists())
    {
        throw NotFoundError("LocalServer::start", "Could not find Doomsday.app");
    }
    cmd.append(bin);

#elif WIN32
    if (!d->appPath.isEmpty())
    {
        bin = d->appPath / "doomsday-server.exe";
    }
    if (!bin.exists())
    {
        bin = App::executableDir() / "doomsday-server.exe";
    }
    cmd.append(bin);
    cmd.append("-basedir");
    cmd.append(bin.fileNamePath() / "..");

#else // UNIX
    if (!d->appPath.isEmpty())
    {
        bin = d->appPath / "doomsday-server";
    }
    if (!bin.exists())
    {
        bin = App::executableDir() / "doomsday-server";
    }
    if (!bin.exists()) bin = "doomsday-server"; // Perhaps it's on the path?
    cmd.append(bin);
#endif

    cmd.append("-userdir");
    cmd.append(d->userDir);
    cmd.append("-errors");
    cmd.append(ERROR_LOG_NAME);
    cmd.append("-game");
    cmd.append(gameMode);
    cmd.append("-cmd");
    cmd.append(Stringf("net-ip-port %d", port));

    if (!d->name.isEmpty())
    {
        cmd.append("-cmd");
        cmd.append("server-name \"" + d->name + "\"");
    }

    for (const String &opt : additionalOptions) cmd.append(opt);

    LOG_NET_NOTE("Starting local server on port %i using game mode '%s'")
            << port << gameMode;
    LOG_NET_MSG("Server command line options:");
    for (size_t i = 1; i < cmd.size(); ++i)
    {
        LOG_NET_MSG(" %3d: %s") << i << cmd.at(i);
    }

    d->proc = tF::make_ref(cmd.executeProcess());
}

void LocalServer::stop()
{
    if (isRunning())
    {
        LOG_NET_NOTE("Stopping local server on port %i") << d->port;
        kill_Process(d->proc);
    }
}

duint16 LocalServer::port() const
{
    return d->port;
}

bool LocalServer::isRunning() const
{
    if (!d->proc) return false;
    return isRunning_Process(d->proc) == iTrue;
}

Link *LocalServer::openLink()
{
    if (!isRunning()) return nullptr;
    return new Link(Stringf("localhost:%d", d->port), 30.0);
}

NativePath LocalServer::errorLogPath() const
{
    return d->userDir / ERROR_LOG_NAME;
}

} // namespace network
