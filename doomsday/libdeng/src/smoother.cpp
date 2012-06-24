/**
 * @file smoother.cpp
 * Interpolator for smoothing out a movement curve. @ingroup math
 *
 * The original movement path is composed out of discrete 3D points. Smoother
 * calculates the points in between.
 *
 * This is used by the server to approximate the movement path of the clients'
 * player mobjs.
 *
 * The movement of the smoother is guaranteed to not make jumps back
 * into the past or change its course once the interpolation has begun
 * between two points.
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#include <stdlib.h>
#include <string.h>

#include "smoother.h"
#include <de/Log>

/**
 * Timed 3D point in space.
 */
typedef struct pos_s {
    coord_t xyz[3];
    float time;
    boolean onFloor;    // Special Z handling: should be on the floor.
} pos_t;

/** Points from the future. */
#define SM_NUM_POINTS 2

/**
 * The smoother contains the data necessary to determine the
 * coordinates on the smooth path at a certain point in time.
 * It is assumed that time always moves forward.
 */
struct smoother_s {
    pos_t points[SM_NUM_POINTS];  // Future points.
    pos_t past, now;  // Current interpolation.
    float at;         // Current position in time for the smoother.
    float maxDeltaBetweenPastAndNow;

#ifdef _DEBUG
    float prevEval[2], prevAt;
#endif
};

Smoother* Smoother_New()
{
    Smoother* sm = static_cast<Smoother*>(calloc(sizeof(Smoother), 1));
    return sm;
}

void Smoother_SetMaximumPastNowDelta(Smoother* sm, float delta)
{
    DENG_ASSERT(sm);
    sm->maxDeltaBetweenPastAndNow = delta;
}

void Smoother_Delete(Smoother* sm)
{
    DENG_ASSERT(sm);
    free(sm);
}

void Smoother_Debug(const Smoother* sm)
{
    DENG_ASSERT(sm);
    LOG_DEBUG("Smoother_Debug: [past=%3.3f / now=%3.3f / future=%3.3f] at=%3.3f")
            << sm->past.time << sm->now.time << sm->points[0].time << sm->at;
}

static boolean Smoother_IsValid(const Smoother* sm)
{
    DENG_ASSERT(sm);
    if(sm->past.time == 0 || sm->now.time == 0)
    {
        // We don't have valid data.
        return false;
    }
    return true;
}

void Smoother_Clear(Smoother* sm)
{
    float maxDelta;

    DENG_ASSERT(sm);

    maxDelta = sm->maxDeltaBetweenPastAndNow;
    memset(sm, 0, sizeof(*sm));
    sm->maxDeltaBetweenPastAndNow = maxDelta;
}

void Smoother_AddPosXY(Smoother* sm, float time, coord_t x, coord_t y)
{
    Smoother_AddPos(sm, time, x, y, 0, false);
}

void Smoother_AddPos(Smoother* sm, float time, coord_t x, coord_t y, coord_t z, boolean onFloor)
{
    pos_t* last;
    DENG_ASSERT(sm);

    // Is it the same point?
    last = &sm->points[SM_NUM_POINTS - 1];
    if(last->time == time)
    {
        if(last->xyz[VX] == x && last->xyz[VY] == y && last->xyz[VZ] == z)
        {
            // Ignore it.
            return;
        }

        // Readjusting a previously set value?
        goto replaceLastPoint;
    }

    if(time <= sm->now.time)
    {
        // The new point would be in the past, this is no good.
#ifdef _DEBUG
        LOG_DEBUG("Smoother_AddPos: DISCARDING new pos, time=%f, now=%f.") << time << sm->now.time;
#endif
        Smoother_Clear(sm);
        return;
    }

    // If we are about to discard an unused future point, we will force
    // the current interpolation into the future.
    if(Smoother_IsValid(sm) && sm->points[0].time > sm->now.time)
    {
        coord_t mid[3];
        float remaining;

        // Move the past forward in time so that the interpolation remains continuous.
        remaining = sm->now.time - sm->at;

        Smoother_Evaluate(sm, mid);
        sm->at = sm->past.time = sm->points[0].time - remaining;
        sm->past.xyz[VX] = mid[VX];
        sm->past.xyz[VY] = mid[VY];
        sm->past.xyz[VZ] = mid[VZ];

        // Replace the now with the point about to be discarded.
        memcpy(&sm->now, &sm->points[0], sizeof(pos_t));
    }

    // Rotate the old points.
    memmove(&sm->points[0], &sm->points[1], sizeof(pos_t) * (SM_NUM_POINTS - 1));

replaceLastPoint:
    last = &sm->points[SM_NUM_POINTS - 1];
    last->time = time;
    last->xyz[VX] = x;
    last->xyz[VY] = y;
    last->xyz[VZ] = z;
    last->onFloor = onFloor;

    // Is this the first one?
    if(sm->now.time == 0)
    {
        sm->at = time;
        memcpy(&sm->past, last, sizeof(pos_t));
        memcpy(&sm->now, last, sizeof(pos_t));
    }
}

boolean Smoother_EvaluateComponent(const Smoother* sm, int component, coord_t* v)
{
    coord_t xyz[3];

    DENG_ASSERT(component >= 0 && component < 3);
    DENG_ASSERT(v != 0);

    if(!Smoother_Evaluate(sm, xyz)) return false;

    *v = xyz[component];
    return true;
}

boolean Smoother_Evaluate(const Smoother* sm, coord_t* xyz)
{
    const pos_t* past;
    const pos_t* now;
    float t;
    int i;

    DENG_ASSERT(sm);
    past = &sm->past;
    now = &sm->now;

    if(!Smoother_IsValid(sm))
    {
/*#ifdef _DEBUG
        Con_Message("Smoother_Evaluate: sm=%p not valid!\n", sm);
#endif*/
        return false;
    }

    if(sm->at < past->time)
    {
        // Before our time.
        xyz[VX] = past->xyz[VX];
        xyz[VY] = past->xyz[VY];
        xyz[VZ] = past->xyz[VZ];
/*#if _DEBUG
        Con_Message("Smoother_Evaluate: falling behind\n");
        ((Smoother*)sm)->prevEval[0] = xyz[0];
        ((Smoother*)sm)->prevEval[1] = xyz[1];
#endif*/
        return true;
    }
    //DENG_ASSERT(sm->at <= now->time);
    if(now->time <= past->time)
    {
        // Too far in the ever-shifting future.
        xyz[VX] = now->xyz[VX];
        xyz[VY] = now->xyz[VY];
        xyz[VZ] = now->xyz[VZ];
/*#if _DEBUG
        Con_Message("Smoother_Evaluate: stalling\n");
        ((Smoother*)sm)->prevEval[0] = xyz[0];
        ((Smoother*)sm)->prevEval[1] = xyz[1];
#endif*/
        return true;
    }

    // We're somewhere between past and now.
    t = (sm->at - past->time) / (now->time - past->time);
    for(i = 0; i < 3; ++i)
    {
        // Linear interpolation.
        xyz[i] = now->xyz[i] * t + past->xyz[i] * (1-t);
    }

/*#ifdef _DEBUG
    {
        float dt = sm->at - sm->prevAt;
        //Smoother_Debug(sm);
        if(dt > 0)
        {
            float diff[2] = { xyz[0] - sm->prevEval[0], xyz[1] - sm->prevEval[1] };
            Con_Message("Smoother_Evaluate: [%05.3f] diff = %+06.3f  %+06.3f\n", dt, diff[0]/dt, diff[1]/dt);
            ((Smoother*)sm)->prevEval[0] = xyz[0];
            ((Smoother*)sm)->prevEval[1] = xyz[1];
        }
        ((Smoother*)sm)->prevAt = sm->at;
    }
#endif*/
    return true;
}

boolean Smoother_IsOnFloor(const Smoother* sm)
{
    DENG_ASSERT(sm);

    const pos_t* past = &sm->past;
    const pos_t* now = &sm->now;

    if(!Smoother_IsValid(sm)) return false;
    return (past->onFloor && now->onFloor);
}

boolean Smoother_IsMoving(const Smoother* sm)
{
    DENG_ASSERT(sm);

    const pos_t* past = &sm->past;
    const pos_t* now = &sm->now;

    // The smoother is moving if the current past and present are different
    // points in time and space.
    return sm->at >= past->time && sm->at <= now->time && past->time < now->time &&
            (!INRANGE_OF(past->xyz[VX], now->xyz[VX], SMOOTHER_MOVE_EPSILON) ||
             !INRANGE_OF(past->xyz[VY], now->xyz[VY], SMOOTHER_MOVE_EPSILON) ||
             !INRANGE_OF(past->xyz[VZ], now->xyz[VZ], SMOOTHER_MOVE_EPSILON));
}

void Smoother_Advance(Smoother* sm, float period)
{
    int i;

    DENG_ASSERT(sm);

    if(period <= 0) return;

    sm->at += period;

    // Did we go past the present?
    while(sm->at > sm->now.time)
    {
        int j = -1;

        // The present has become the past.
        memcpy(&sm->past, &sm->now, sizeof(pos_t));

        // Choose the next point from the future.
        for(i = 0; i < SM_NUM_POINTS; ++i)
        {
            if(sm->points[i].time > sm->now.time)
            {
                // Use this one.
                j = i;
                break;
            }
        }
        if(j < 0)
        {
            // No points were applicable. We need to stop here until
            // new points are received.
            sm->at = sm->now.time;
            break;
        }
        else
        {
            memcpy(&sm->now, &sm->points[j], sizeof(pos_t));
        }
    }

    if(sm->maxDeltaBetweenPastAndNow > 0 &&
       sm->now.time - sm->past.time > sm->maxDeltaBetweenPastAndNow)
    {
        // Refresh the past.
        sm->past.time = sm->now.time;
    }

    if(sm->at < sm->past.time)
    {
        // Don't fall too far back.
        sm->at = sm->past.time;
    }
}
