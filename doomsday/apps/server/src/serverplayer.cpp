/** @file serverplayer.cpp  Server-side player state.
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

#include "serverplayer.h"

using namespace de;

DE_PIMPL_NOREF(ServerPlayer)
{
    /// Each client has their own pool for deltas.
    pool_t deltaPool;

    Impl()
    {
        zap(deltaPool);
    }
};

ServerPlayer::ServerPlayer()
    : remoteUserId(0)
    , enterTime(0)
    , shakePing(0)
    , handshake(false)
    , lastTransmit(0)
    , fov(90)
    , ready(false)
    , d(new Impl)
{}

bool ServerPlayer::isConnected() const
{
    return remoteUserId != 0;
}

pool_t &ServerPlayer::deltaPool()
{
    return d->deltaPool;
}
