/** @file player.h  Base class for player state.
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

#ifndef LIBDOOMSDAY_PLAYER_H
#define LIBDOOMSDAY_PLAYER_H

#include "libdoomsday.h"
#include "network/pinger.h"
#include <de/legacy/types.h>
#include <de/legacy/smoother.h>

/// Maximum length of a player name.
#define PLAYERNAMELEN       81

/// Normally one for the weapon and one for the muzzle flash.
#define DDMAXPSPRITES       2

/// Psprite states. @ingroup player
enum {
    DDPSP_BOBBING,
    DDPSP_FIRE,
    DDPSP_DOWN,
    DDPSP_UP
};

/**
 * @defgroup pspriteFlags PSprite Flags
 * @ingroup player apiFlags
 */
///@{
#define DDPSPF_FULLBRIGHT 0x1
///@}

/// Player sprite. @ingroup player
typedef struct {
    struct state_s *statePtr;
    int             tics;
    float           alpha;
    float           pos[2];
    byte            flags; /// @ref pspriteFlags
    int             state;
    float           offset[2];
} ddpsprite_t;

#define LOOKDIRMAX  110.0f // 85 degrees

/// Player lookdir (view pitch) conversion to degrees. @ingroup player
#define LOOKDIR2DEG(x)  ((x) * 85.0f/LOOKDIRMAX)

/// Player lookdir (view pitch) conversion to radians. @ingroup player
#define LOOKDIR2RAD(x)  (LOOKDIR2DEG(x)/180*DD_PI)

LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC short P_LookDirToShort(float lookDir);
LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC float P_ShortToLookDir(short s);

struct mobj_s;

typedef struct fixcounters_s {
    int             angles;
    int             origin;
    int             mom;
} fixcounters_t;

typedef struct ddplayer_s {
    float           forwardMove; // Copied from player brain (read only).
    float           sideMove; // Copied from player brain (read only).
    struct mobj_s  *mo; // Pointer to a (game specific) mobj.
    angle_t         appliedBodyYaw; // Body yaw currently applied
    float           lookDir; // For mouse look.
    int             fixedColorMap; // Can be set to REDCOLORMAP, etc.
    int             extraLight; // So gun flashes light up areas.
    int             inGame; // Is this player in game?
    int             flags;
    float           filterColor[4]; // RGBA filter for the camera.
    fixcounters_t   fixCounter;
    fixcounters_t   fixAcked;
    angle_t         lastAngle; // For calculating turndeltas.
    ddpsprite_t     pSprites[DDMAXPSPRITES]; // Player sprites.
    void*           extraData; // Pointer to any game-specific data.
} ddplayer_t;

#ifdef __cplusplus

#include <de/scripting/iobject.h>
#include <de/record.h>

namespace world { class World; }

/**
 * Base class for player state: common functionality shared by both the server
 * and the client.
 *
 * @todo Revise: this entire class is public as part of libdoomday, and therefore
 * can be accessed by plugins, too. Previously only ddplayer_t has been made
 * public. Some members could be moved into an internal, engine-only private class,
 * if necessary. -jk
 */
class LIBDOOMSDAY_PUBLIC Player : public de::IObject
{
public:
    // ID number. Each client has a unique ID number.
    ident_t id;

    // The name of the player.
    char name[PLAYERNAMELEN];

    byte extraLightCounter; ///< Num tics to go till extraLight is disabled.
    int extraLight;
    int targetExtraLight;

    // View console. Which player this client is viewing?
    int viewConsole;

public:
    Player();

    virtual ~Player();

    void initBindings();

    virtual void setWorld(world::World *world);

    ddplayer_t &publicData();
    const ddplayer_t &publicData() const;

    /**
     * Determines if the player is in the game and has a mobj.
     */
    bool isInGame() const;

    /**
     * Returns the player's namespace.
     */
    const de::Record &info() const;

    /**
     * Returns the player's namespace.
     */
    de::Record &info();

    Smoother *smoother();

    Pinger &pinger();
    const Pinger &pinger() const;

    /**
     * Called once on every tick.
     *
     * @param elapsed  Amount of time elapsed since the previous call.
     */
    virtual void tick(timespan_t elapsed);

    DE_CAST_METHODS()

    // Implements IObject.
    de::Record &objectNamespace();
    const de::Record &objectNamespace() const;

private:
    DE_PRIVATE(d)
};

#endif // __cplusplus

#endif // LIBDOOMSDAY_PLAYER_H
