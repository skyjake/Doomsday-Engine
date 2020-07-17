/** @file localserver.h  Starting and stopping local servers.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#pragma once

#include "link.h"
#include <de/error.h>
#include <de/nativepath.h>

namespace network {

using namespace de;

/**
 * Utility for starting and stopping local servers.
 *
 * @ingroup shell
 */
class LIBDOOMSDAY_PUBLIC LocalServer
{
public:
    /// Failed to locate the server executable. @ingroup errors
    DE_ERROR(NotFoundError);

public:
    LocalServer();

    /**
     * Sets the name of the server.
     *
     * @param name  Name.
     */
    void setName(const String &name);

    void setApplicationPath(const NativePath &path);

    void start(duint16 port, const String &gameMode,
               const StringList &additionalOptions = {},
               const NativePath &runtimePath = "");

    void stop();

    duint16 port() const;

    bool isRunning() const;

    /**
     * Opens a link for communicating with the server. The returned link will
     * initially be in the Link::Connecting state.
     *
     * @return Link to the local server. Caller gets ownership.
     */
    Link *openLink();

    /**
     * Reads the path of the error log. This is useful after a failed server start.
     *
     * @return Native path of the error log.
     */
    NativePath errorLogPath() const;

private:
    DE_PRIVATE(d)
};

} // namespace network
