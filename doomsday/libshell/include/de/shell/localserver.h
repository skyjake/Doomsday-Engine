/** @file localserver.h  Starting and stopping local servers.
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

#ifndef LIBSHELL_LOCALSERVER_H
#define LIBSHELL_LOCALSERVER_H

#include "Link"
#include <de/Error>
#include <de/NativePath>
#include <QStringList>

namespace de {
namespace shell {

/**
 * Utility for starting and stopping local servers.
 */
class LIBSHELL_PUBLIC LocalServer
{
public:
    /// Failed to locate the server executable. @ingroup errors
    DENG2_ERROR(NotFoundError);

public:
    LocalServer();

    virtual ~LocalServer();

    void start(duint16 port,
               String const &gameMode,
               QStringList additionalOptions = QStringList(),
               NativePath const &runtimePath = "");

    void stop();

    /**
     * Returns the Link for communicating with the server.
     */
    //Link *openLink();

private:
    struct Instance;
    Instance *d;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_LOCALSERVER_H
