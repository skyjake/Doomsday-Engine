/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Calculation of smooth movement paths. This is used by the server
 * to approximate the movement path of the clients' player mobjs.
 *
 * The movement of the smoother is guaranteed to not make jumps back
 * into the past or change its course once the interpolation has begun
 * between two points.
 */

#include <stdlib.h>
#include <string.h>

#include "de_base.h"
#include "smoother.h"

/**
 * Timed 3D point in space.
 */
typedef struct pos_s {
    float xyz[3];
    float time;
    boolean onFloor;    // Special Z handling: should be on the floor.
} pos_t;

enum {
    SMOOTHER_PAST = 0,
    SMOOTHER_PRESENT = 1,
    SMOOTHER_FUTURE = 2
};

/**
 * The smoother contains the data necessary to determine the 
 * coordinates on the smooth path at a certain point in time.
 * It is assumed that time always moves forward.
 */
struct smoother_s {
    pos_t   past, now, future;
    float   at;         // Current position in time for the smoother.
};

Smoother* Smoother_New()
{
    Smoother* sm = calloc(sizeof(Smoother), 1);
    return sm;
}

void Smoother_Destruct(Smoother* sm)
{
    free(sm);
}

void Smoother_Clear(Smoother* sm)
{
    memset(sm, 0, sizeof(*sm));
}

void Smoother_AddPos(Smoother* sm, float time, float x, float y, float z, boolean onFloor)
{
    if(!sm) return;

/*#ifdef _DEBUG
    Con_Message("Smoother_AddPos: new time %f, smoother at %f\n", time, sm->at);
#endif*/

    if(time <= sm->now.time)
    {
        // The new point would be in the past, this is no good.
#ifdef _DEBUG
        Con_Message("Smoother_AddPos: DISCARDING new pos.\n");
#endif
        Smoother_Clear(sm);
        return;
    }

    // Set the future.
    sm->future.time = time;
    sm->future.xyz[VX] = x;
    sm->future.xyz[VY] = y;
    sm->future.xyz[VZ] = z;
    sm->future.onFloor = onFloor;

    // Is this the first one?
    if(sm->now.time == 0)
    {
        sm->at = time;
        memcpy(&sm->past, &sm->future, sizeof(pos_t));
        memcpy(&sm->now, &sm->future, sizeof(pos_t));
    }
}

static boolean Smoother_IsValid(const Smoother* sm)
{
    if(sm->past.time == 0 || sm->now.time == 0)
    {
        // We don't have valid data.
        return false;
    }
    return true;
}

boolean Smoother_Evaluate(const Smoother* sm, float* xyz)
{
    const pos_t* past = &sm->past;
    const pos_t* now = &sm->now;
    float t;
    int i;

    if(!Smoother_IsValid(sm))
    {
#ifdef _DEBUG
        Con_Message("Smoother_Evaluate: sm=%p not valid!\n", sm);
#endif
        return false;
    }

    if(sm->at < past->time)
    {
        // Before our time.
        xyz[VX] = past->xyz[VX];
        xyz[VY] = past->xyz[VY];
        xyz[VZ] = past->xyz[VZ];
        return true;
    }
    if(sm->at > now->time || now->time <= past->time)
    {
        // Too far in the ever-shifting future.
        xyz[VX] = now->xyz[VX];
        xyz[VY] = now->xyz[VY];
        xyz[VZ] = now->xyz[VZ];
        return true;
    }

    // We're somewhere between past and now.
    t = (sm->at - past->time) / (now->time - past->time);
    for(i = 0; i < 3; ++i)
    {
        // Linear interpolation.
        xyz[i] = now->xyz[i] * t + past->xyz[i] * (1-t);
    }
    return true;
}

boolean Smoother_IsOnFloor(const Smoother* sm)
{
    const pos_t* past = &sm->past;
    const pos_t* now = &sm->now;

    if(!Smoother_IsValid(sm)) return false;
    return (past->onFloor && now->onFloor);
}

boolean Smoother_IsMoving(const Smoother* sm)
{
    const pos_t* past = &sm->past;
    const pos_t* now = &sm->now;

#define SMOOTHER_MOVE_EPSILON .001f

    // The smoother is moving if the current past and present are different
    // points in time and space.
    return sm->at >= past->time && sm->at <= now->time && past->time < now->time &&
            (!INRANGE_OF(past->xyz[VX], now->xyz[VX], SMOOTHER_MOVE_EPSILON) ||
             !INRANGE_OF(past->xyz[VY], now->xyz[VY], SMOOTHER_MOVE_EPSILON) ||
             !INRANGE_OF(past->xyz[VZ], now->xyz[VZ], SMOOTHER_MOVE_EPSILON));
}

void Smoother_Advance(Smoother* sm, float period)
{
    sm->at += period;  

    if(sm->at < sm->past.time)
    {
        // Don't fall too far back.
        sm->at = sm->past.time;
    }

    // Did we go past the present?
    if(sm->at >= sm->now.time)
    {
        // The present has become the past.
        memcpy(&sm->past, &sm->now, sizeof(pos_t));

        // Is the future valid?
        if(sm->future.time > sm->now.time)
        {
            memcpy(&sm->now, &sm->future, sizeof(pos_t));
        }
        else
        {
            // Stay at the threshold rather than going to an unknown future.
            sm->at = sm->now.time;
        }
    }

/*#ifdef _DEBUG
    Con_Message("Smoother_Advance: sm=%p at=%f past=%f now=%f fut=%f\n", sm,
                sm->at, sm->past.time,
                sm->now.time,
                sm->future.time);
#endif*/
}

void Smoother_Debug(const Smoother* sm)
{
    Con_Message("Smoother_Debug: [past=%3.3f / now=%3.3f / future=%3.3f] at=%3.3f\n",
                sm->past.time, sm->now.time, sm->future.time, sm->at);
}
