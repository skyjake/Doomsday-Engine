/** @file sv_pool.h Delta Pools.
 * @ingroup server
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef __DOOMSDAY_SERVER_POOL_H__
#define __DOOMSDAY_SERVER_POOL_H__

#include "dd_share.h"
#include "world/p_object.h"
#include "sv_missile.h"

#include <doomsday/world/plane.h>
#include <doomsday/world/polyobj.h>
#include <doomsday/world/surface.h>

#ifdef __cplusplus
extern "C" {
#endif

// OR'd with the type number when resending Unacked deltas.
#define DT_RESENT               0x80

// Mobj Delta Control flags (not included directly in the frame).
#define MDFC_NULL               0x010000 // The delta is not defined.
#define MDFC_CREATE             0x020000 // Mobj didn't exist before.
#define MDFC_TRANSLUCENCY       0x040000 // Mobj has translucency.
#define MDFC_FADETARGET         0x080000 // Mobj is fading to/from visible/invisible
#define MDFC_TYPE               0x100000 // Mobj type specified in delta.
#define MDFC_ON_FLOOR           0x200000 // Mobj Z is floorZ.

// The flags that are not included when a mobj is the viewpoint.
#define MDF_CAMERA_EXCLUDE      0x0e00

// The flags that are not included for hidden mobjs.
#define MDF_DONTDRAW_EXCLUDE    0x0ec0

// The flags that are not included when a player is the viewpoint.
#define PDF_CAMERA_EXCLUDE      0x001e

// The flags that are not included when a player is not the viewpoint.
#define PDF_NONCAMERA_EXCLUDE   0x70de

typedef enum deltastate_e {
    DELTA_NEW,
    DELTA_UNACKED
} deltastate_t;

/**
 * All delta structures begin the same way (with a delta_t).
 * That way they can all be linked into the same hash table.
 */
typedef struct delta_s {
    // Links to the next and previous delta in the hash.
    struct delta_s* next, *prev;

    // The ID number and type determine the entity this delta applies to.
    deltatype_t     type;
    uint            id;

    // The priority score tells how badly the delta needs to be sent to
    // the client.
    float           score;

    // Deltas can be either New or Unacked. New deltas haven't yet been sent.
    deltastate_t    state;

    // ID of the delta set. Assigned when the delta is sent to a client.
    // All deltas in the same frame update have the same set ID.
    // Clients acknowledge complete sets (and then the whole set is removed).
    byte            set;

    // Resend ID of this delta. Assigned when the delta is first resent.
    // Zero means there is no resend ID.
    byte            resend;

    // System time when the delta was sent.
    uint            timeStamp;

    int             flags;
} delta_t;

typedef mobj_t  dt_mobj_t;

typedef struct mobjdelta_s {
    delta_t         delta; // The header.
    dt_mobj_t       mo; // The data of the delta.

    mobjdelta_s() { de::zap(mo.thinker); }
} mobjdelta_t;

typedef struct {
    thid_t          mobj;
    char            forwardMove;
    char            sideMove;
    int             angle;
    int             turnDelta;
    coord_t         friction;
    int             extraLight;
    int             fixedColorMap;
    int             filter;
    int             clYaw;
    float           clPitch;
    ddpsprite_t     psp[2]; // Player sprites.
} dt_player_t;

typedef struct {
    delta_t         delta; // The header.
    dt_player_t     player;
} playerdelta_t;

typedef struct {
    world::Material *material;
    float           rgba[4]; // Surface color tint and alpha
    int             blendMode;
} dt_surface_t;

typedef struct {
    dt_surface_t    surface;
    coord_t         height;
    coord_t         target; // Target height.
    coord_t         speed; // Move speed.
} dt_plane_t;

typedef enum {
    PLN_FLOOR,
    PLN_CEILING
} dt_planetype_t;

typedef struct {
    float           lightLevel;
    float           rgb[3];
    uint            planeCount;
    dt_plane_t      planes[2];
} dt_sector_t;

typedef struct {
    delta_t         delta;
    dt_sector_t     sector;
} sectordelta_t;

typedef struct {
    delta_t         delta;
} lumpdelta_t;

typedef struct {
    dt_surface_t    top;
    dt_surface_t    middle;
    dt_surface_t    bottom;
    byte            lineFlags; // note: only a byte!
    byte            flags; // Side flags.
} dt_side_t;

typedef struct {
    delta_t         delta;
    dt_side_t       side;
} sidedelta_t;

typedef struct {
    float           dest[2];
    float           speed;
    angle_t         destAngle;
    angle_t         angleSpeed;
} dt_poly_t;

typedef struct {
    delta_t         delta;
    dt_poly_t       po;
} polydelta_t;

typedef struct {
    delta_t         delta; // id = Emitter identifier (mobjid/sectoridx)
    int             sound; // Sound ID
    const mobj_t *  mobj;
    float           volume;
} sounddelta_t;

/**
 * One hash table holds all the deltas in a pool.
 * (delta ID number) & (mask) is the key.
 */
#define POOL_HASH_SIZE              1024
#define POOL_HASH_FUNCTION_MASK     0x3ff

/**
 * The missile record contains an entry for each missile mobj that
 * the client has acknowledged. Since missiles move predictably,
 * we do not need to sent the coordinates in every delta. The record
 * is checked every time a missile delta is added to a pool.
 */
#define POOL_MISSILE_HASH_SIZE      256

typedef struct deltalink_s {
    // Links to the first and last delta in the hash key.
    struct delta_s* first, *last;
} deltalink_t;

/**
 * When calculating priority scores, this struct is used to store
 * information about the owner of the pool.
 */
typedef struct ownerinfo_s {
    struct pool_s*  pool;
    coord_t         origin[3]; // Distance is the most important factor
    angle_t         angle; // Angle can change rapidly => not very important
    float           speed;
    uint            ackThreshold; // Expected ack time in milliseconds
} ownerinfo_t;

/**
 * Each client has a delta pool.
 */
typedef struct pool_s {
    // True if the first frame has not yet been sent.
    dd_bool         isFirst;

    // The number of the console this pool belongs to. (i.e. player number)
    uint            owner;
    ownerinfo_t     ownerInfo;

    // The set ID numbers are generated using this value. It's
    // incremented after each transmitted set.
    byte            setDealer;

    // The resend ID numbers are generated using this value. It's incremented
    // for each resent delta. Zero is not used.
    byte            resendDealer;

    // The delta hash table holds all kinds of deltas.
    deltalink_t     hash[POOL_HASH_SIZE];

    // The missile record is used to detect when the mobj coordinates need
    // not be sent.
    mislink_t       misHash[POOL_MISSILE_HASH_SIZE];

    // The priority queue (a heap). Built when the pool contents are rated.
    // Contains pointers to deltas in the hash. Becomes invalid when deltas
    // are removed from the hash!
    int             queueSize;
    int             allocatedSize;
    delta_t**       queue;
} pool_t;

void            Sv_InitPools(void);
void            Sv_ShutdownPools(void);
void            Sv_DrainPool(uint clientNumber);
void            Sv_InitPoolForClient(uint clientNumber);
void            Sv_MobjRemoved(thid_t id);
void            Sv_PlayerRemoved(uint clientNumber);
void            Sv_GenerateFrameDeltas(void);
dd_bool         Sv_IsFrameTarget(uint clientNumber);
uint            Sv_GetTimeStamp(void);
pool_t*         Sv_GetPool(uint clientNumber);
void            Sv_RatePool(pool_t* pool);
delta_t*        Sv_PoolQueueExtract(pool_t* pool);
void            Sv_AckDeltaSet(uint clientNumber, int set, byte resent);
uint            Sv_CountUnackedDeltas(uint clientNumber);

/**
 * Adds a new sound delta to the selected client pools. As the starting of a
 * sound is in itself a 'delta-like' event, there is no need for comparing or
 * to have a register.
 *
 * @note Assumes no two sounds with the same ID play at the same time
 *       from the same origin at once.
 *
 * @param soundId        Sound ID (from defs).
 * @param emitter        Mobj that is emitting the sound, or @c NULL.
 * @param sourceSector   For sector-emitted sounds, the source sector.
 * @param sourcePoly     For polyobj-emitted sounds, the source object.
 * @param sourcePlane    For plane-emitted sounds, the source object.
 * @param sourceSurface  For surface-emitted sounds, the source surface.
 * @param volume         Volume at which to play the sound, or zero for stop-sound.
 * @param isRepeating    Is the sound repeating?
 * @param clientsMask    Each bit corresponds a client number;
 *                       -1 if all clients should receive the delta.
 */
void Sv_NewSoundDelta(int soundId, const mobj_t *emitter, world::Sector *sourceSector,
    Polyobj *sourcePoly, world::Plane *sourcePlane, world::Surface *sourceSurface,
    float volume, dd_bool isRepeating, int clientsMask);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
