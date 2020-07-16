/** @file serverplayer.h  Server-side player state.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/id.h>
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
    /// Identifier of the RemoteUser instance of this client.
    de::Id::Type remoteUserId;

    /// Seconds when the client entered the game (Sys_GetRealSeconds()).
    double enterTime;

    /// Clients are pinged by the server when they join the game.
    /// This is the ping in milliseconds for this client.
    unsigned int shakePing;

    /// If true, the server will send the player a handshake. The client must
    /// acknowledge it before this flag is turned off.
    bool handshake;

    int lastTransmit;

    /// Field of view. Used in determining visible mobjs (default: 90).
    float fov;

    /// Server uses this to determine whether it's OK to send game packets
    /// to the client.
    bool ready;

public:
    ServerPlayer();

    /// Is this client connected? (Might not be in the game yet.)
    bool isConnected() const;

    pool_t &deltaPool();
    
private:
    DE_PRIVATE(d)
};

#endif // SERVER_SERVERPLAYER_H
