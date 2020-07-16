/** @file player.cpp  Base class for player state.
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

#include "doomsday/player.h"
#include "doomsday/world/world.h"

#include <de/scripting/scriptsystem.h>

using namespace de;

DE_PIMPL_NOREF(Player)
{
    world::World *world = nullptr;
    ddplayer_t publicData;
    Record info;
    Smoother *smoother = Smoother_New();
    Pinger pinger;

    Impl()
    {
        zap(publicData);
        zap(pinger);
    }

    ~Impl()
    {
        Smoother_Delete(smoother);
    }
};

Player::Player()
    : id(0)
    , extraLightCounter(0)
    , extraLight(0)
    , targetExtraLight(0)
    , viewConsole(0)
    , d(new Impl)
{
    zap(name);
}

Player::~Player()
{}

void Player::initBindings()
{
    objectNamespace().addSuperRecord(ScriptSystem::builtInClass("App", "Player"));
}

void Player::setWorld(world::World *world)
{
    if (world)
    {
        d->world = world;
    }
}

ddplayer_t &Player::publicData()
{
    return d->publicData;
}

const ddplayer_t &Player::publicData() const
{
    return d->publicData;
}

bool Player::isInGame() const
{
    return d->publicData.inGame && d->publicData.mo != nullptr;
}

const Record &Player::info() const
{
    return d->info;
}

Record &Player::info()
{
    return d->info;
}

Smoother *Player::smoother()
{
    return d->smoother;
}

Pinger &Player::pinger()
{
    return d->pinger;
}

const Pinger &Player::pinger() const
{
    return d->pinger;
}

void Player::tick(timespan_t /*elapsed*/)
{}

Record &Player::objectNamespace()
{
    return d->info;
}

const Record &Player::objectNamespace() const
{
    return d->info;
}

short P_LookDirToShort(float lookDir)
{
    int dir = int( lookDir/110.f * DDMAXSHORT );

    if (dir < DDMINSHORT) return DDMINSHORT;
    if (dir > DDMAXSHORT) return DDMAXSHORT;
    return (short) dir;
}

float P_ShortToLookDir(short s)
{
    return s / float( DDMAXSHORT ) * 110.f;
}
