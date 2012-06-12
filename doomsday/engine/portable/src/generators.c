/**
 * @file generators.c
 * Generators. @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "dd_zone.h"

#include "generators.h"

typedef struct listnode_s {
    struct listnode_s* next;
    ptcgen_t* gen;
} listnode_t;

struct generators_s {
    ptcgen_t* activeGens[GENERATORS_MAX];

    listnode_t* linkStore;
    uint linkStoreCursor;
    uint linkStoreSize;

    // Array of list heads containing links from linkStore to generators in activeGens.
    listnode_t** lists;
    uint listsSize;
};

Generators* Generators_New(uint listCount)
{
    Generators* gens = Z_Malloc(sizeof(*gens), PU_MAP, 0);
    if(!gens) Con_Error("Generators_New: Failed on allocation of %lu bytes for new Generators instance.", (unsigned long) sizeof(*gens));

    memset(gens->activeGens, 0, sizeof(gens->activeGens));

    gens->listsSize = listCount;
    gens->lists = Z_Calloc(sizeof(listnode_t*) * listCount, PU_MAP, 0);

    // We can link 64 generators each into four lists each before running out of links.
    gens->linkStoreSize = 4 * GENERATORS_MAX;
    gens->linkStore = Z_Malloc(sizeof(listnode_t) * gens->linkStoreSize, PU_MAP, 0);
    gens->linkStoreCursor = 0;

    return gens;
}

void Generators_Delete(Generators* gens)
{
    assert(gens);
    Z_Free(gens->lists);
    Z_Free(gens->linkStore);
    Z_Free(gens);
}

void Generators_Clear(Generators* gens)
{
    assert(gens);
    Generators_EmptyLists(gens);
    memset(gens->activeGens, 0, sizeof(gens->activeGens));
}

ptcgen_t* Generators_Generator(Generators* gens, ptcgenid_t id)
{
    assert(gens);
    if(id >= 0 && id < GENERATORS_MAX)
        return gens->activeGens[id];
    return NULL; // Not found.
}

ptcgenid_t Generators_GeneratorId(Generators* gens, const ptcgen_t* gen)
{
    assert(gens);
    if(gen)
    {
        ptcgenid_t i;
        for(i = 0; i < GENERATORS_MAX; ++i)
        {
            if(gens->activeGens[i] == gen)
                return i;
        }
    }
    return -1; // Not found.
}

ptcgenid_t Generators_NextAvailableId(Generators* gens)
{
    ptcgenid_t i;
    assert(gens);
    /// @optimize Cache this result.
    for(i = 0; i < GENERATORS_MAX; ++i)
    {
        if(!gens->activeGens[i])
            return i;
    }
    return -1; // None available.
}

/**
 * Returns an unused link from the linkStore.
 */
static listnode_t* Generators_NewLink(Generators* gens)
{
    assert(gens);
    if(gens->linkStoreCursor < gens->linkStoreSize)
        return &gens->linkStore[gens->linkStoreCursor++];

    VERBOSE( Con_Message("Generators_NewLink: Exhausted store.\n") );
    return NULL;
}

ptcgen_t* Generators_Unlink(Generators* gens, ptcgen_t* gen)
{
    ptcgenid_t i;
    assert(gens);

    for(i = 0; i < GENERATORS_MAX; ++i)
    {
        if(gens->activeGens[i] == gen)
        {
            gens->activeGens[i] = 0;
            break;
        }
    }
    return gen;
}

ptcgen_t* Generators_Link(Generators* gens, ptcgenid_t slot, ptcgen_t* gen)
{
    assert(gens);
    assert(slot < GENERATORS_MAX);
    // Sanity check - generator is not already linked.
    assert(Generators_GeneratorId(gens, gen) < 0);

    gens->activeGens[slot] = gen;
    return gen;
}

ptcgen_t* Generators_LinkToList(Generators* gens, ptcgen_t* gen, uint listIndex)
{
    listnode_t* link, *it;
    assert(gens);

    // Sanity check - generator is one from this collection.
    assert(Generators_GeneratorId(gens, gen) >= 0);

    // Must check that it isn't already there...
    assert(listIndex < gens->listsSize);
    for(it = gens->lists[listIndex]; it; it = it->next)
    {
        if(it->gen == gen) return gen; // No, no...
        /*Con_Error("Generators_LinkToList: Attempted repeat link of generator %p to list %u.", (void*)gen, listIndex);
        exit(1); // Unreachable.*/
    }

    // We need a new link.
    link = Generators_NewLink(gens);
    if(link)
    {
        link->gen = gen;
        link->next = gens->lists[listIndex];
        gens->lists[listIndex] = link;
    }
    return gen;
}

void Generators_EmptyLists(Generators* gens)
{
    assert(gens);
    if(!gens->lists) return;

    memset(gens->lists, 0, sizeof(*gens->lists) * gens->listsSize);
    gens->linkStoreCursor = 0;
}

int Generators_Iterate(Generators* gens, int (*callback) (ptcgen_t*, void*), void* parameters)
{
    ptcgenid_t i;
    assert(gens);
    for(i = 0; i < GENERATORS_MAX; ++i)
    {
        int result;

        // Only consider active generators.
        if(!gens->activeGens[i]) continue;

        result = callback(gens->activeGens[i], parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

int Generators_IterateList(Generators* gens, uint listIndex,
    int (*callback) (ptcgen_t*, void*), void* parameters)
{
    listnode_t* it;
    assert(gens);
    for(it = gens->lists[listIndex]; it; it = it->next)
    {
        int result = callback(it->gen, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}
