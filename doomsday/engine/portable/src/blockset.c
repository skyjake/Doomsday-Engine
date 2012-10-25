/**\file blockset.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <stdlib.h>
#include <assert.h> // Define NDEBUG in release builds.

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"

#include "blockset.h"

typedef struct blockset_block_s {
    /// Number of used elements.
    size_t count;

    /// Block of memory where elements are.
    void* elements;
} blockset_block_t;

/**
 * Allocate a new block of memory to be used for linear object allocations.
 *
 * @param set  Block set into which the new block is added.
 */
static void addBlockToSet(blockset_t* set)
{
    assert(set);
    {
    blockset_block_t* block = 0;

    // Get a new block by resizing the blocks array. This is done relatively
    // seldom, since there is a large number of elements per each block.
    set->_blocks = realloc(set->_blocks, sizeof(blockset_block_t) * ++set->_blockCount);

    // Initialize the block's data.
    block = &set->_blocks[set->_blockCount - 1];
    block->elements = malloc(set->_elementSize * set->_elementsPerBlock);
    block->count = 0;
    }
}

void* BlockSet_Allocate(blockset_t* set)
{
    assert(set);
    {
    blockset_block_t* block = &set->_blocks[set->_blockCount - 1];

    // When this is called, there is always an available element in the topmost
    // block. We will return it.
    void* element = ((byte*)block->elements) + (set->_elementSize * block->count);

    // Reserve the element in this block.
    ++block->count;

    // If we run out of space in the topmost block, add a new one.
    if(block->count == set->_elementsPerBlock)
    {
        // Just being cautious: adding a new block invalidates existing
        // pointers to the blocks.
        block = 0;

        addBlockToSet(set);
    }

    // Maintain a running total of the number of used elements in all blocks.
    ++set->_elementsInUse;

    return element;
    }
}

blockset_t* BlockSet_New(size_t sizeOfElement, size_t batchSize)
{
    blockset_t* set;

    if(sizeOfElement == 0)
        Con_Error("Attempted BlockSet::Construct with invalid sizeOfElement (==0).");

    if(batchSize == 0)
        Con_Error("Attempted BlockSet::Construct with invalid batchSize (==0).");

    // Allocate the blockset.
    set = calloc(1, sizeof(*set));
    set->_elementsPerBlock = batchSize;
    set->_elementSize = sizeOfElement;
    set->_elementsInUse = 0;

    // Allocate the first block right away.
    addBlockToSet(set);

    return set;
}

size_t BlockSet_Count(blockset_t* set)
{
    assert(set);
    return set->_elementsInUse;
}

void BlockSet_Delete(blockset_t* set)
{
    assert(set);

    // Free the elements from each block.
    { size_t i;
    for(i = 0; i < set->_blockCount; ++i)
        free(set->_blocks[i].elements);
    }

    free(set->_blocks);
    free(set);
}
