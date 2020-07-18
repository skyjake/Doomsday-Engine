/** @file net.h  Network subsystem.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "libdoomsday.h"
#include <de/ibytearray.h>
#include <de/legacy/types.h>
#include <de/transmitter.h>

typedef struct netstate_s
{
    dd_bool firstUpdate;
    int     netGame;  // a networked game is in progress
    int     isServer; // this computer is an open server
    int     isClient; // this computer is a client
    float   simulatedLatencySeconds;
    int     gotFrame; // a frame packet has been received
} netstate_t;

LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC netstate_t netState;

/**
 * Network subsystem for game-level communication between the server and the client.
 */
class LIBDOOMSDAY_PUBLIC Net
{
public:
    Net();

    void setTransmitterLookup(const std::function<de::Transmitter *(int player)> &);

    /**
     * Sends data to a player. Available during multiplayer games.
     *
     * @param player  Player number (0 is always the server).
     * @param data    Data to send.
     */
    void sendDataToPlayer(int player, const de::IByteArray &data);

private:
    DE_PRIVATE(d)
};
