/** @file serverplayer.h  Server-side player state.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef SERVER_SERVERPLAYER_H
#define SERVER_SERVERPLAYER_H

#include <de/Id>
#include <doomsday/player.h>
#include "server/sv_pool.h"

/**
 * Server-side player state: delta pool, client bookkeeping information.
 *
 * @ingroup server
 */
class ServerPlayer : public Player
{
public:
    // Identifier of the RemoteUser instance of this client.
    de::Id::Type remoteUserId;

public:
    ServerPlayer();

    /// Is this client connected? (Might not be in the game yet.)
    bool isConnected() const;

    pool_t &deltaPool();
    
private:
    DENG2_PRIVATE(d)
};

#endif // SERVER_SERVERPLAYER_H
