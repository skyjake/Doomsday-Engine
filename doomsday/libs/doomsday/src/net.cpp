/** @file net.cpp  Network subsystem.
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

#include "doomsday/net.h"
#include "doomsday/players.h"
#include "doomsday/doomsdayapp.h"
#include <de/list.h>

using namespace de;

netstate_t netState = {
    true, // first update
    0, 0, 0, 0.0f, 0
};

DE_PIMPL(Net)
{
    std::function<Transmitter *(int player)> transmitter;

    Impl(Public *i) : Base(i)
    {}
};

Net::Net()
    : d(new Impl(this))
{}

void Net::setTransmitterLookup(const std::function<Transmitter *(int player)> &func)
{
    d->transmitter = func;
}

void Net::sendDataToPlayer(int player, const IByteArray &data)
{
    DE_ASSERT(d->transmitter);
    List<Transmitter *> dests;
    if (netState.isServer)
    {
        if (player >= 0 && player < DDMAXPLAYERS)
        {
            if (auto *x = d->transmitter(player))
            {
                dests << x;
            }
        }
        else
        {
            // Broadcast to all non-local players, using recursive calls.
            for (int i = 0; i < DDMAXPLAYERS; ++i)
            {
                if (auto *x = d->transmitter(i))
                {
                    dests << x;
                }
            }
        }
    }
    else
    {
        dests << d->transmitter(player);
    }
    for (auto *transmit : dests)
    {
        *transmit << data;
    }
}
