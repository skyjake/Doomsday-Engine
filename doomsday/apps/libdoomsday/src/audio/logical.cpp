/** @file logical.cpp  Logical Sound Manager.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/audio/logical.h"
#include <de/timer.h>
#include <de/memoryzone.h>

using namespace de;

// The logical sounds hash table uses sound IDs as keys.
#define LOGIC_HASH_SIZE     ( 64 )

#define PURGE_INTERVAL      ( 2000 )  ///< 2 seconds

struct logicsound_t
{
    logicsound_t *next, *prev;

    dint id;
    mobj_t *origin;
    duint endTime;
    bool isRepeating;
};

struct logichash_t
{
    logicsound_t *first, *last;
};
static logichash_t logicHash[LOGIC_HASH_SIZE];

static dd_bool logicalOneSoundPerEmitter;

static duint (*logicalSoundLengthCallback)(dint);

static logichash_t &getLogicHash(dint id)
{
    return ::logicHash[(duint) id % LOGIC_HASH_SIZE];
}

/**
 * Create and link a new logical sound hash table node.
 */
static logicsound_t *newLogicSound(dint id)
{
    auto *node = reinterpret_cast<logicsound_t *>(Z_Calloc(sizeof(logicsound_t), PU_MAP, nullptr));

    logichash_t &hash = getLogicHash(id);
    if(hash.last)
    {
        hash.last->next = node;
        node->prev = hash.last;
    }
    hash.last = node;
    if(!hash.first)
        hash.first = node;

    node->id = id;
    return node;
}

/**
 * Unlink and destroy a logical sound hash table node.
 */
static void destroyLogicSound(logicsound_t *node)
{
    if(!node) return;

    logichash_t &hash = getLogicHash(node->id);
    if(hash.first == node)
        hash.first = node->next;
    if(hash.last == node)
        hash.last = node->prev;

    if(node->next)
        node->next->prev = node->prev;
    if(node->prev)
        node->prev->next = node->next;

#ifdef DENG2_DEBUG
    std::memset(node, 0xDD, sizeof(*node));
#endif
    Z_Free(node);
}

void Sfx_Logical_SetOneSoundPerEmitter(dd_bool enabled)
{
    ::logicalOneSoundPerEmitter = enabled;
}

void Sfx_Logical_SetSampleLengthCallback(duint (*callback)(dint))
{
    ::logicalSoundLengthCallback = callback;
}

void Sfx_InitLogical()
{
    // Memory in the hash table is PU_MAP, so this is acceptable.
    std::memset(::logicHash, 0, sizeof(::logicHash));
}

void Sfx_StartLogical(dint id, mobj_t *origin, dd_bool isRepeating)
{
    DENG2_ASSERT(::logicalSoundLengthCallback != nullptr);

    duint const length = (isRepeating ? 1 : ::logicalSoundLengthCallback(id));
    if(!length)
    {
        // This is not a valid sound.
        return;
    }

    if(origin && ::logicalOneSoundPerEmitter)
    {
        // Stop all previous sounds from this origin (only one per origin).
        Sfx_StopLogical(0, origin);
    }

    id &= ~DDSF_FLAG_MASK;

    logicsound_t *node = newLogicSound(id);
    node->origin      = origin;
    node->isRepeating = CPP_BOOL(isRepeating);
    node->endTime     = Timer_RealMilliseconds() + length;
}

dint Sfx_StopLogical(dint id, mobj_t *origin)
{
    dint stopCount = 0;

    if(id)
    {
        logicsound_t *it = getLogicHash(id).first;
        while(it)
        {
            logicsound_t *next = it->next;
            if(it->id == id && it->origin == origin)
            {
                destroyLogicSound(it);
                stopCount++;
            }
            it = next;
        }
    }
    else
    {
        // Browse through the entire hash.
        for(dint i = 0; i < LOGIC_HASH_SIZE; ++i)
        {
            logicsound_t *it = logicHash[i].first;
            while(it)
            {
                logicsound_t *next = it->next;
                if(!origin || it->origin == origin)
                {
                    destroyLogicSound(it);
                    stopCount++;
                }
                it = next;
            }
        }
    }

    return stopCount;
}

void Sfx_PurgeLogical()
{
    static duint lastTime = 0;

    // Too soon for a purge?
    duint const nowTime = Timer_RealMilliseconds();
    if(nowTime - lastTime < PURGE_INTERVAL) return;

    // Peform the purge now.
    lastTime = nowTime;

    // Check all sounds in the hash.
    for(dint i = 0; i < LOGIC_HASH_SIZE; ++i)
    {
        logicsound_t *it = logicHash[i].first;
        while(it)
        {
            logicsound_t *next = it->next;
            if(!it->isRepeating && it->endTime < nowTime)
            {
                // This has stopped.
                destroyLogicSound(it);
            }
            it = next;
        }
    }
}

dd_bool Sfx_IsPlaying(dint id, mobj_t *origin)
{
    duint const nowTime = Timer_RealMilliseconds();

    if(id)
    {
        for(logicsound_t *it = getLogicHash(id).first; it; it = it->next)
        {
            if(it->id == id && it->origin == origin &&
               (it->endTime > nowTime || it->isRepeating))
            {
                // This one is still playing.
                return true;
            }
        }
    }
    else if(origin)
    {
        // Check if the origin is playing any sound.
        for(dint i = 0; i < LOGIC_HASH_SIZE; ++i)
        for(logicsound_t *it = logicHash[i].first; it; it = it->next)
        {
            if(it->origin == origin && (it->endTime > nowTime || it->isRepeating))
            {
                // This one is playing.
                return true;
            }
        }
    }

    // The sound was not found.
    return false;
}
