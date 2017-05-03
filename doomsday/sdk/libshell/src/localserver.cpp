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

#include "de/shell/LocalServer"
#include "de/shell/Link"
#include "de/shell/DoomsdayInfo"
#include <de/CommandLine>
#include <QCoreApplication>
#include <QDir>

#if !defined (DENG_IOS)

namespace de {
namespace shell {

static String const ERROR_LOG_NAME = "doomsday-errors.out";

DENG2_PIMPL_NOREF(LocalServer)
{
    Link *link = nullptr;
    NativePath appPath; // where to find the server
    duint16 port = 0;
    String name;
    NativePath userDir;
    QProcess *proc = nullptr; // not deleted until stopped

    ~Impl()
    {
        if (proc && proc->state() == QProcess::NotRunning)
        {
            delete proc;
        }
    }
};

LocalServer::LocalServer() : d(new Impl)
{}

void LocalServer::setName(String const &name)
{
    d->name = name;
    d->name.replace("\"", "\\\""); // for use on command line
}

void LocalServer::setApplicationPath(NativePath const &path)
{
    d->appPath = path;
}

void LocalServer::start(duint16 port,
                        String const &gameMode,
                        QStringList additionalOptions,
                        NativePath const &runtimePath)
{
    d->port = port;

    d->userDir = runtimePath;

    if (d->userDir.isEmpty())
    {
        // Default runtime location.
        d->userDir = DoomsdayInfo::defaultServerRuntimeFolder();
    }

    // Get rid of a previous error log in this location.
    QDir(d->userDir).remove(ERROR_LOG_NAME);

    DENG2_ASSERT(d->link == 0);

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
        bin = NativePath(qApp->applicationDirPath()) / "../MacOS/doomsday-server";
    }
    if (!bin.exists())
    {
        // Yet another possibility: Doomsday Shell.app -> Doomsday.app
        // App folder randomization means this is only useful in developer builds, though.
        bin = NativePath(qApp->applicationDirPath()) /
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
        bin = NativePath(qApp->applicationDirPath()) / "doomsday-server.exe";
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
        bin = NativePath(qApp->applicationDirPath()) / "doomsday-server";
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
    cmd.append("net-ip-port " + String::number(port));

    if (!d->name.isEmpty())
    {
        cmd.append("-cmd");
        cmd.append("server-name \"" + d->name + "\"");
    }

    foreach (String opt, additionalOptions) cmd.append(opt);

    LOG_NET_NOTE("Starting local server on port %i using game mode '%s'")
            << port << gameMode;

    d->proc = cmd.executeProcess();
}

void LocalServer::stop()
{
    if (isRunning())
    {
        LOG_NET_NOTE("Stopping local server on port %i") << d->port;
        d->proc->kill();
    }
}

duint16 LocalServer::port() const
{
    return d->port;
}

bool LocalServer::isRunning() const
{
    if (!d->proc) return false;
    return (d->proc->state() != QProcess::NotRunning);
}

Link *LocalServer::openLink()
{
    if (!isRunning()) return nullptr;
    return new Link(String("localhost:%1").arg(d->port), 30);
}

NativePath LocalServer::errorLogPath() const
{
    return d->userDir / ERROR_LOG_NAME;
}
   
} // namespace shell
} // namespace de

#endif
