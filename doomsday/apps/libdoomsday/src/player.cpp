/** @file player.cpp  Base class for player state.
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

#include "doomsday/player.h"

using namespace de;

DENG2_PIMPL_NOREF(Player)
{
    ddplayer_t publicData;
    Record info;

    Instance()
    {
        zap(publicData);
    }
};

Player::Player()
    : extraLightCounter(0)
    , extraLight(0)
    , targetExtraLight(0)
    , d(new Instance)
{}

Player::~Player()
{}

ddplayer_t &Player::publicData()
{
    return d->publicData;
}

ddplayer_t const &Player::publicData() const
{
    return d->publicData;
}

Record const &Player::info() const
{
    return d->info;
}

Record &Player::info()
{
    return d->info;
}

short P_LookDirToShort(float lookDir)
{
    int dir = int( lookDir/110.f * DDMAXSHORT );

    if(dir < DDMINSHORT) return DDMINSHORT;
    if(dir > DDMAXSHORT) return DDMAXSHORT;
    return (short) dir;
}

float P_ShortToLookDir(short s)
{
    return s / float( DDMAXSHORT ) * 110.f;
}
