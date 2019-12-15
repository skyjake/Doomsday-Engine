/** @file doomsday/players.cpp
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

#include "doomsday/players.h"
#include "doomsday/player.h"

using namespace de;

DENG2_PIMPL_NOREF(Players)
{
    Player *players[DDMAXPLAYERS];

    Impl()
    {
        zap(players);
    }

    ~Impl()
    {
        for (auto *plr : players)
        {
            delete plr;
        }
        zap(players);
    }
};

Players::Players(Constructor playerConstructor) : d(new Impl)
{
    for (auto &plr : d->players)
    {
        plr = playerConstructor();
        DENG2_ASSERT(is<Player>(plr));
    }
}

Player &Players::at(int index) const
{
    DENG2_ASSERT(index >= 0);
    DENG2_ASSERT(index < DDMAXPLAYERS);
    return *d->players[index];
}

int Players::count() const
{
    return DDMAXPLAYERS;
}

LoopResult Players::forAll(std::function<LoopResult (Player &)> func) const
{
    for (auto &plr : d->players)
    {
        if (auto result = func(*plr))
        {
            return result;
        }
    }
    return LoopContinue;
}

int Players::indexOf(Player const *player) const
{
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if (d->players[i] == player)
        {
            return i;
        }
    }
    return -1;
}

int Players::indexOf(ddplayer_s const *publicData) const
{
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if (&d->players[i]->publicData() == publicData)
        {
            return i;
        }
    }
    return -1;
}

void Players::initBindings()
{
    for (auto *plr : d->players)
    {
        plr->initBindings();
    }
}
