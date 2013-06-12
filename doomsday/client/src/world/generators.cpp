/** @file generators.cpp Generators.
 * @ingroup world
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <de/memoryzone.h>

//#include "de_platform.h"
//#include "de_console.h"
//#include "dd_main.h"

#include "world/generators.h"

namespace de {

struct ListNode
{
    ListNode *next;
    ptcgen_t *gen;
};

DENG2_PIMPL(Generators)
{
    ptcgen_t *activeGens[GENERATORS_MAX];

    uint linkStoreSize;
    ListNode *linkStore;
    uint linkStoreCursor;

    uint listsSize;
    // Array of list heads containing links from linkStore to generators in activeGens.
    ListNode **lists;

    Instance(Public *i, uint listCount)
        : Base(i),
          // We can link 64 generators each into four lists each before running out of links.
          linkStoreSize(4 * GENERATORS_MAX),
          linkStore((ListNode *) Z_Malloc(sizeof(ListNode) * linkStoreSize, PU_MAP, 0)),
          linkStoreCursor(0),
          listsSize(listCount),
          lists((ListNode **) Z_Calloc(sizeof(ListNode*) * listsSize, PU_MAP, 0))
    {
        zap(activeGens);
    }

    ~Instance()
    {
        Z_Free(lists);
        Z_Free(linkStore);
    }

    /**
     * Returns an unused link from the linkStore.
     */
    ListNode *newLink()
    {
        if(linkStoreCursor < linkStoreSize)
            return &linkStore[linkStoreCursor++];

        LOG_AS("Generators::newLink");
        LOG_WARNING("Exhausted generator link storage.");
        return 0;
    }
};

Generators::Generators(uint listCount)
    : d(new Instance(this, listCount))
{}

void Generators::clear()
{
    emptyLists();
    zap(d->activeGens);
}

ptcgen_t *Generators::generator(ptcgenid_t id) const
{
    if(id >= 0 && id < GENERATORS_MAX)
    {
        return d->activeGens[id];
    }
    return 0; // Not found.
}

Generators::ptcgenid_t Generators::generatorId(ptcgen_t const *gen) const
{
    if(gen)
    {
        for(ptcgenid_t i = 0; i < GENERATORS_MAX; ++i)
        {
            if(d->activeGens[i] == gen)
                return i;
        }
    }
    return -1; // Not found.
}

Generators::ptcgenid_t Generators::nextAvailableId() const
{
    /// @todo Optimize: Cache this result.
    for(ptcgenid_t i = 0; i < GENERATORS_MAX; ++i)
    {
        if(!d->activeGens[i])
            return i;
    }
    return -1; // None available.
}

ptcgen_t *Generators::unlink(ptcgen_t *gen)
{
    if(gen)
    {
        for(ptcgenid_t i = 0; i < GENERATORS_MAX; ++i)
        {
            if(d->activeGens[i] == gen)
            {
                d->activeGens[i] = 0;
                break;
            }
        }
    }
    return gen;
}

ptcgen_t *Generators::link(ptcgen_t *gen, ptcgenid_t slot)
{
    if(gen && slot < GENERATORS_MAX)
    {
        // Sanity check - generator is not already linked.
        DENG_ASSERT(generatorId(gen) < 0);

        d->activeGens[slot] = gen;
    }
    return gen;
}

ptcgen_t *Generators::linkToList(ptcgen_t *gen, uint listIndex)
{
    DENG_ASSERT(listIndex < d->listsSize);

    // Sanity check - generator is one from this collection.
    DENG_ASSERT(generatorId(gen) >= 0);

    // Must check that it isn't already there...
    for(ListNode *it = d->lists[listIndex]; it; it = it->next)
    {
        if(it->gen == gen)
        {
            LOG_AS("Generators::linkToList");
            LOG_DEBUG("Attempted repeat link of generator %p to list %u.")
                    << de::dintptr(gen) << listIndex;

            return gen; // No, no...
        }
    }

    // We need a new link.
    if(ListNode *link = d->newLink())
    {
        link->gen = gen;
        link->next = d->lists[listIndex];
        d->lists[listIndex] = link;
    }

    return gen;
}

void Generators::emptyLists()
{
    if(!d->lists) return;

    std::memset(d->lists, 0, sizeof(*d->lists) * d->listsSize);
    d->linkStoreCursor = 0;
}

int Generators::iterate(int (*callback) (ptcgen_t *, void *), void *parameters)
{
    int result = false; // Continue iteration.
    for(ptcgenid_t i = 0; i < GENERATORS_MAX; ++i)
    {
        // Only consider active generators.
        if(!d->activeGens[i]) continue;

        result = callback(d->activeGens[i], parameters);
        if(result) break;
    }
    return result;
}

int Generators::iterateList(uint listIndex, int (*callback) (ptcgen_t *, void *), void *parameters)
{
    int result = false; // Continue iteration.
    for(ListNode *it = d->lists[listIndex]; it; it = it->next)
    {
        result = callback(it->gen, parameters);
        if(result) break;
    }
    return result;
}

} // namespace de
