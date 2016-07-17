/** @file clientplayer.h  Client-side player state.
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

#ifndef CLIENT_CLIENTPLAYER_H
#define CLIENT_CLIENTPLAYER_H

#include <doomsday/player.h>
#include "render/viewports.h"
#include "lzss.h" // legacy demo code

struct ConsoleEffectStack;
namespace render { class PlayerWeaponAnimator; }

/**
 * Information about a client player.
 *
 * @todo This is probably partially obsolete? Rename/revamp. -jk
 */
typedef struct clplayerstate_s {
    thid_t clMobjId;
    float forwardMove;
    float sideMove;
    int angle;
    angle_t turnDelta;
    int friction;
    int pendingFixes;
    thid_t pendingFixTargetClMobjId;
    angle_t pendingAngleFix;
    float pendingLookDirFix;
    coord_t pendingOriginFix[3];
    coord_t pendingMomFix[3];
} clplayerstate_t;

struct DemoTimer
{
    bool first;
    de::dint begintime;
    bool canwrite;  ///< @c false until Handshake packet.
    de::dint cameratimer;
    de::dint pausetime;
    de::dfloat fov;
};

/**
 * Client-side player state.
 */
class ClientPlayer : public Player
{
public:
    // Demo recording file (being recorded if not NULL).
    LZFILE *demo;
    bool recording;
    bool recordPaused;

    /// @c true if the player is in the void. (Not entirely accurate so should not be used
    /// for anything critical).
    bool inVoid;

public:
    ClientPlayer();

    viewdata_t &viewport();
    viewdata_t const &viewport() const;

    clplayerstate_t &clPlayerState();
    clplayerstate_t const &clPlayerState() const;

    ConsoleEffectStack &fxStack();
    ConsoleEffectStack const &fxStack() const;

    render::PlayerWeaponAnimator &playerWeaponAnimator();

    DemoTimer &demoTimer();

    void tick(timespan_t elapsed);

    /**
     * Sets the id of the currently active weapon of this player. This is used for
     * looking up assets related to the weapon (e.g., "model.weapon.(id)").
     *
     * @param id  Weapon id, as defined by the game.
     */
    void setWeaponAssetId(de::String const &id);

    void weaponStateChanged(struct state_s const *state);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_CLIENTPLAYER_H
