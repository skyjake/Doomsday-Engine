/** @file sessionmetadata.h  Saved game session metadata.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_SESSIONMETADATA
#define LIBCOMMON_SESSIONMETADATA

#include "common.h"
#include <de/Observers>
#include <de/String>

/**
 * Game session metadata (serialized to savegames).
 * @ingroup libcommon
 */
struct SessionMetadata
{
#if !__JHEXEN__
    // Info data about players present (or not) in the game session.
    typedef byte Players[MAXPLAYERS];
#endif

    de::String userDescription;
    uint sessionId;
    int magic;
    int version;
    de::String gameIdentityKey;
    Uri *mapUri;
    GameRuleset gameRules;
#if !__JHEXEN__
    int mapTime;
    Players players;
#endif

    SessionMetadata();
    SessionMetadata(SessionMetadata const &other);
    ~SessionMetadata();

    static SessionMetadata *fromReader(Reader *reader);

    void write(Writer *writer) const;
    void read(Reader *reader);

    de::String asText() const;
};

#endif // LIBCOMMON_SESSIONMETADATA
