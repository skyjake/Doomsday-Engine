/**
 * @file memoryblockset.c
 * Set of memory blocks allocated from the zone.
 *
 * @authors Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/legacy/memoryblockset.h"
#include "de/legacy/memory.h"

typedef struct blockset_block_s {
    size_t count;   ///< Number of used elements.
    void *elements; ///< Block of memory where elements are.
} blockset_block_t;

/**
 * Allocate a new block of memory to be used for linear object allocations.
 *
 * @param set  Block set into which the new block is added.
 */
static void addBlockToSet(blockset_t *set)
{
    blockset_block_t *block = 0;

    DE_ASSERT(set);

    // Get a new block by resizing the blocks array. This is done relatively
    // seldom, since there is a large number of elements per each block.
    set->_blocks = M_Realloc(set->_blocks, sizeof(blockset_block_t) * ++set->_blockCount);

    // Initialize the block's data.
    block = &set->_blocks[set->_blockCount - 1];
    block->elements = M_Malloc(set->_elementSize * set->_elementsPerBlock);
    block->count = 0;
}

void *BlockSet_Allocate(blockset_t *set)
{
    DE_ASSERT(set);
    {
    blockset_block_t *block = &set->_blocks[set->_blockCount - 1];

    // When this is called, there is always an available element in the topmost
    // block. We will return it.
    void *element = ((byte *)block->elements) + (set->_elementSize * block->count);

    // Reserve the element in this block.
    ++block->count;

    // If we run out of space in the topmost block, add a new one.
    if (block->count == set->_elementsPerBlock)
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

blockset_t *BlockSet_New(size_t sizeOfElement, size_t batchSize)
{
    blockset_t *set;

    DE_ASSERT(sizeOfElement > 0);
    DE_ASSERT(batchSize > 0);

    // Allocate the blockset.
    set = M_Calloc(sizeof(*set));
    set->_elementsPerBlock = batchSize;
    set->_elementSize = sizeOfElement;
    set->_elementsInUse = 0;

    // Allocate the first block right away.
    addBlockToSet(set);

    return set;
}

size_t BlockSet_Count(blockset_t *set)
{
    DE_ASSERT(set);

    return set->_elementsInUse;
}

void BlockSet_Delete(blockset_t *set)
{
    size_t i;

    DE_ASSERT(set);

    // Free the elements from each block.
    for (i = 0; i < set->_blockCount; ++i)
        M_Free(set->_blocks[i].elements);

    M_Free(set->_blocks);
    M_Free(set);
}
