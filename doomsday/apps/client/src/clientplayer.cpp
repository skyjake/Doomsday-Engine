/** @file clientplayer.cpp  Client-side player state.
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

#include "clientplayer.h"
#include "render/consoleeffect.h"
#include "render/playerweaponanimator.h"
#include "ui/viewcompositor.h"
#include "def_share.h"

#include <doomsday/world/world.h>

using namespace de;

DE_PIMPL(ClientPlayer)
, DE_OBSERVES(world::World, MapChange)
{
    ViewCompositor     viewCompositor;
    viewdata_t         viewport;
    ConsoleEffectStack effects;
    render::PlayerWeaponAnimator playerWeaponAnimator;
    clplayerstate_t    clPlayerState;
    DemoTimer          demoTimer;

    const state_t *lastPSpriteState = nullptr;
    String weaponAssetId;

    Impl(Public *i)
        : Base(i)
        , playerWeaponAnimator(i)
    {
        zap(clPlayerState);
        zap(demoTimer);
    }

    void worldMapChanged()
    {
        // Reset the weapon animator when the map changes.
        if (playerWeaponAnimator.assetId())
        {
            playerWeaponAnimator.setAsset(playerWeaponAnimator.assetId());
        }
    }
};

ClientPlayer::ClientPlayer()
    : recording(false)
    , recordPaused(false)
    , d(new Impl(this))
{}

void ClientPlayer::setWorld(world::World *world)
{
    Player::setWorld(world);
    if (world)
    {
        world->audienceForMapChange() += d;
    }
}

ViewCompositor &ClientPlayer::viewCompositor()
{
    return d->viewCompositor;
}

viewdata_t &ClientPlayer::viewport()
{
    return d->viewport;
}

const viewdata_t &ClientPlayer::viewport() const
{
    return d->viewport;
}

clplayerstate_t &ClientPlayer::clPlayerState()
{
    return d->clPlayerState;
}

const clplayerstate_t &ClientPlayer::clPlayerState() const
{
    return d->clPlayerState;
}

ConsoleEffectStack &ClientPlayer::fxStack()
{
    return d->effects;
}

const ConsoleEffectStack &ClientPlayer::fxStack() const
{
    return d->effects;
}

render::PlayerWeaponAnimator &ClientPlayer::playerWeaponAnimator()
{
    return d->playerWeaponAnimator;
}

DemoTimer &ClientPlayer::demoTimer()
{
    return d->demoTimer;
}

void ClientPlayer::tick(timespan_t elapsed)
{
    if (!isInGame()) return;

    d->playerWeaponAnimator.advanceTime(elapsed);
}

void ClientPlayer::setWeaponAssetId(const String &id)
{
    if (id != d->weaponAssetId)
    {
        LOG_RES_VERBOSE("Weapon asset: %s") << id;
        d->weaponAssetId = id;
        d->playerWeaponAnimator.setAsset("model.weapon." + id);
        d->playerWeaponAnimator.stateChanged(d->lastPSpriteState);
    }
}

void ClientPlayer::weaponStateChanged(const state_t *state)
{
    if (state != d->lastPSpriteState)
    {
        d->lastPSpriteState = state;
        d->playerWeaponAnimator.stateChanged(state);
    }
}
