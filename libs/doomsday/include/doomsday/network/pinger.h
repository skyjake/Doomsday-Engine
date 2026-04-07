/** @file doomsday/network/pinger.h
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

#ifndef LIBDOOMSDAY_NETWORK_PINGER_H
#define LIBDOOMSDAY_NETWORK_PINGER_H

#include "../libdoomsday.h"

#define PING_TIMEOUT        1000   // Ping timeout (ms).
#define MAX_PINGS           10

struct Pinger
{
    // High tics when ping was sent (0 if pinger not used).
    int sent;

    // A record of the pings (negative time: no response).
    float times[MAX_PINGS];

    // Total number of pings and the current one.
    int total;
    int current;
};

#endif // LIBDOOMSDAY_NETWORK_PINGER_H

