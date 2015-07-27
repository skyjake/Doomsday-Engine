/** @file clientplayer.cpp  Client-side player state.
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

#include "clientplayer.h"
#include "render/consoleeffect.h"
#include "render/playerweaponanimator.h"

using namespace de;

DENG2_PIMPL(ClientPlayer)
{
    viewdata_t           viewport;
    ConsoleEffectStack   effects;
    PlayerWeaponAnimator playerWeaponAnimator;
    clplayerstate_t      clPlayerState;
    DemoTimer            demoTimer;

    String weaponAssetId;

    Instance(Public *i)
        : Base(i)
        , playerWeaponAnimator(i)
    {
        zap(viewport);
        zap(clPlayerState);
        zap(demoTimer);
    }

    void prepareAssets()
    {
        // Is there a model for the weapon?
        qDebug() << "-=- looking for" << "model.weapon." + weaponAssetId;
    }
};

ClientPlayer::ClientPlayer()
    : demo(nullptr)
    , recording(false)
    , recordPaused(false)
    , d(new Instance(this))
{}

viewdata_t &ClientPlayer::viewport()
{
    return d->viewport;
}

viewdata_t const &ClientPlayer::viewport() const
{
    return d->viewport;
}

clplayerstate_t &ClientPlayer::clPlayerState()
{
    return d->clPlayerState;
}

clplayerstate_t const &ClientPlayer::clPlayerState() const
{
    return d->clPlayerState;
}

ConsoleEffectStack &ClientPlayer::fxStack()
{
    return d->effects;
}

ConsoleEffectStack const &ClientPlayer::fxStack() const
{
    return d->effects;
}

PlayerWeaponAnimator &ClientPlayer::playerWeaponAnimator()
{
    return d->playerWeaponAnimator;
}

DemoTimer &ClientPlayer::demoTimer()
{
    return d->demoTimer;
}

void ClientPlayer::tick(timespan_t elapsed)
{
    if(!isInGame()) return;

    d->playerWeaponAnimator.advanceTime(elapsed);
}

void ClientPlayer::setWeaponAssetId(String const &id)
{
    if(id != d->weaponAssetId)
    {
        d->weaponAssetId = id;
        d->prepareAssets();
    }
}
