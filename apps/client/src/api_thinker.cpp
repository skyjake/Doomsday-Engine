/** @file api_thinker.cpp
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "api_thinker.h"
#include "world/p_object.h"

#include <doomsday/world/map.h>
#include <doomsday/world/world.h>
#include <doomsday/world/thinkers.h>

using namespace de;
using World = world::World;

#undef Thinker_Init
void Thinker_Init()
{
    if (World::get().hasMap())
    {
        World::get().map().thinkers().initLists(0x1);  // Init the public thinker lists.
    }
}

static void unlinkThinkerFromList(thinker_t *th)
{
    th->next->prev = th->prev;
    th->prev->next = th->next;
}

#undef Thinker_Run
void Thinker_Run()
{
    /// @todo fixme: Do not assume the current map.
    if (!World::get().hasMap()) return;

    World::get().map().thinkers().forAll(0x1 | 0x2, [](thinker_t *th) {
        try
        {
            if (Thinker_InStasis(th)) return LoopContinue; // Skip.

            // Time to remove it?
            if (th->function == thinkfunc_t(-1))
            {
                unlinkThinkerFromList(th);

                if (th->id)
                {
                    // Recycle for reduced allocation overhead.
                    P_MobjRecycle(reinterpret_cast<mobj_t *>(th));
                }
                else
                {
                    // Non-mobjs are just deleted right away.
                    Thinker::destroy(th);
                }
            }
            else if (th->function)
            {
                // Create a private data instance of appropriate type.
                if (!th->d) Thinker_InitPrivateData(th);

                // Public thinker callback.
                th->function(th);

                // Private thinking.
                if (th->d) THINKER_DATA(*th, Thinker::IData).think();
            }
        }
        catch (const Error &er)
        {
            LOG_MAP_WARNING("Thinker %i: %s") << th->id << er.asText();
        }
        return LoopContinue;
    });
}

#undef Thinker_Add
void Thinker_Add(thinker_t *th)
{
    if (!th) return;
    Thinker_Map(*th).thinkers().add(*th);
}

#undef Thinker_Remove
void Thinker_Remove(thinker_t *th)
{
    if (!th) return;
    Thinker_Map(*th).thinkers().remove(*th);
}

#undef Thinker_Iterate
dint Thinker_Iterate(thinkfunc_t func, dint (*callback) (thinker_t *, void *), void *context)
{
    if (!World::get().hasMap()) return false;  // Continue iteration.

    return World::get().map().thinkers().forAll(
        func, 0x1, [&callback, &context](thinker_t *th) { return callback(th, context); });
}

DE_DECLARE_API(Thinker) =
{
    { DE_API_THINKER },
    Thinker_Init,
    Thinker_Run,
    Thinker_Add,
    Thinker_Remove,
    Thinker_Iterate
};
