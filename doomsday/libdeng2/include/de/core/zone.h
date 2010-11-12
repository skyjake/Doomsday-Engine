/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 1999-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright (c) 2006-2009 Daniel Swanson <danij@dengine.net>
 * Copyright (c) 1993-1996 by id Software, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_ZONE_H
#define LIBDENG2_ZONE_H

#include "../deng.h"

#include <list>

namespace de
{
    /**
     * The memory zone allocates raw blocks of memory with low overhead.
     * The zone is composed of multiple memory volumes. New volumes are allocated
     * when there is no space left on the existing ones, allowing the memory zone
     * to grow at runtime.
     *
     * When fast malloc mode is enabled, memory volumes aren't checked for purgable
     * blocks. If the rover block isn't suitable, a new empty volume is created
     * without further checking. This is suitable for cases where lots of blocks
     * are being allocated in a rapid sequence, with no frees in between (e.g.,
     * map setup).
     *
     * Sequences: The MAP_STATIC purge tag has a special purpose.
     * It works like MAP so that it is purged on a per map basis, but
     * blocks allocated as MAP_STATIC should not be freed at any time when the
     * map is being used. Internally, the map-static blocks are linked into
     * sequences so that Z_Malloc knows to skip all of them efficiently. This is
     * possible because no block inside the sequence could be purged by Z_Malloc
     * anyway.
     *
     * There is never any space between memblocks, and there will never be
     * two contiguous free memblocks.
     *
     * The rover can be left pointing at a non-empty block.
     *
     * It is of no value to free a cachable block, because it will get
     * overwritten automatically if needed.
     *
     * Define LIBDENG2_FAKE_MEMORY_ZONE to force all memory blocks to be allocated from
     * the real heap. Useful when debugging memory-related problems.
     *
     * @ingroup core
     */
    class LIBDENG2_API Zone
    {
    public:
        class Batch;
        template <typename T> class Allocator;
        
        /// The specified memory address was outside the zone. @ingroup errors
        DEFINE_ERROR(ForeignError);
        
        /// Invalid purge tag. @ingroup errors
        DEFINE_ERROR(TagError);

        /// Invalid owner. @ingroup errors
        DEFINE_ERROR(OwnerError);    
        
        /// A problem in the structure of the memory zone was detected during
        /// verification. @ingroup errors
        DEFINE_ERROR(ConsistencyError);
        
        /// Purge tags indicate when/if a block can be freed.
        enum PurgeTag {
            UNDEFINED = 0,
            STATIC = 1,     ///< Static entire execution time.
            SOUND = 2,      ///< Static while playing.
            MUSIC = 3,      ///< Static while playing.
            REFRESH_TEXTURE = 11, // Textures/Flats/refresh.
            REFRESH_COLORMAP = 12,
            REFRESH_TRANSLATION = 13,
            REFRESH_SPRITE = 14, 
            PATCH = 15,
            MODEL = 16,
            SPRITE = 20,
            USER1 = 40,
            USER2,
            USER3,
            USER4,
            USER5,
            USER6,
            USER7,
            USER8,
            USER9,
            USER10,
            MAP = 50,   ///< Static until map exited (may still be freed during the map, though).
            MAP_STATIC = 52, ///< Not freed until map exited.
            // Tags >= 100 are purgable whenever needed.
            PURGE_LEVEL = 100,
            CACHE = 101
        };
        
    public:
        Zone();
        ~Zone();

        /**
         * Enables or disables fast malloc mode. Enable for added performance during
         * map setup. Disable fast mode during other times to save memory and reduce
         * fragmentation.
         *
         * @param enabled  true or false.
         */
        void enableFastMalloc(bool enabled = true);
        
        /**
         * Allocates an untyped block of memory.
         * 
         * @param size  Size of the block to allocate.        
         * @param tag   Purge tag. Indicates when the block can be freed.
         * @param user  You can a NULL user if the tag is < PURGE_LEVEL.
         *
         * @return  Pointer to the allocated memory.
         */
        void* alloc(dsize size, PurgeTag tag = STATIC, void* user = 0);

        /**
         * Allocates a typed block of memory.
         * 
         * @param count Number of elements to allocate.        
         * @param tag   Purge tag. Indicates when the block can be freed.
         * @param user  You can a NULL user if the tag is < PURGE_LEVEL.
         *
         * @return  Pointer to the allocated memory.
         */
        template <typename Type>
        Type* allocate(dsize count, PurgeTag tag = STATIC, void* user = 0) {
            return static_cast<Type*>(alloc(sizeof(Type) * count, tag, user));
        }

        /**
         * Allocates and clears an untyped block of memory.
         * 
         * @param size  Size of the block to allocate.        
         * @param tag   Purge tag. Indicates when the block can be freed.
         * @param user  You can a NULL user if the tag is < PURGE_LEVEL.
         */
        void* allocClear(dsize size, PurgeTag tag = STATIC, void* user = 0);

        /**
         * Allocates and clears a typed block of memory.
         * 
         * @param count Numebr of elements to allocate.        
         * @param tag   Purge tag. Indicates when the block can be freed.
         * @param user  You can a NULL user if the tag is < PURGE_LEVEL.
         */
        template <typename Type>
        Type* allocateClear(dsize count, PurgeTag tag = STATIC, void* user = 0) {
            return static_cast<Type*>(allocClear(sizeof(Type) * count, tag, user));
        }

        /**
         * Resizes a block of memory.
         * Only resizes blocks with no user. If a block with a user is
         * reallocated, the user will lose its current block and be set to
         * NULL. Does not change the tag of existing blocks.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         * @param newSize  New size for memory block.
         * @param tagForNewAlloc  Tag used when making a completely new allocation.
         */
        void* resize(void* ptr, dsize newSize, PurgeTag tagForNewAlloc = STATIC);

        template <typename Type>
        Type* resize(Type* ptr, dsize newCount, PurgeTag tagForNewAlloc = STATIC) {
            return static_cast<Type*>(resize(reinterpret_cast<void*>(ptr), 
                sizeof(Type) * newCount, tagForNewAlloc));
        }

        /**
         * Resizes a block of memory so that any new allocated memory is filled with zeroes.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         * @param newSize  New size for memory block.
         * @param tagForNewAlloc  Tag used when making a completely new allocation.
         */
        void* resizeClear(void* ptr, dsize newSize, PurgeTag tagForNewAlloc = STATIC);

        template <typename Type>
        Type* resizeClear(Type* ptr, dsize newCount, PurgeTag tagForNewAlloc = STATIC) {
            return static_cast<Type*>(resizeClear(reinterpret_cast<void*>(ptr), 
                sizeof(Type) * newCount, tagForNewAlloc));
        }

        /**
         * Free memory that was allocated with allocate().
         *
         * @param ptr  Pointer to memory allocated from the zone.
         */
        void free(void *ptr);

        /**
         * Free all memory blocks (in all volumes) with a tag in the specified range.
         * Both @a lowTag and @a highTag are included in the range.
         */
        void purgeRange(PurgeTag lowTag, PurgeTag highTag = CACHE);

        /**
         * Sets the tag of a memory block.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         * @param tag  New purge tag.
         */
        void setTag(void* ptr, PurgeTag tag);

        /**
         * Get the user of a memory block.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         */
        void* getUser(void* ptr) const;
        
        /**
         * Sets the user of a memory block.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         * @param newUser  User for the memory block.
         */
        void setUser(void* ptr, void* newUser);
        
        /**
         * Get the tag of a memory block.
         */
        PurgeTag getTag(void* ptr) const;
        
        /**
         * Calculate the amount of unused memory in all volumes combined.
         */
        dsize availableMemory() const;
        
        /**
         * Check all zone volumes for consistency.
         */
        void verify() const;

        /**
         * Constructs a new  batch allocator. When the allocator is deleted, all
         * the memory blocks allocated by it are automatically freed.
         * The zone retains ownership of the allocator. Batches are deleted automatically
         * based on their purge tags.
         *
         * @see Zone::Batch, deleteBatch()
         *
         * @return  Pointer to the allocator. The zone will automatically delete the
         *      batch if its tag gets purged or when the zone is deleted.
         */
        Batch* newBatch(duint elementSize, duint batchSize, PurgeTag tag = STATIC) {
            _batches.push_back(new Batch(*this, elementSize, batchSize, tag));
            return _batches.back();
        }
        
        /**
         * Constructs a new specialized batch allocator. When the allocator is deleted, all
         * the memory blocks allocated by it are automatically freed.
         * The zone retains ownership of the allocator. Batches are deleted automatically
         * based on their purge tags.
         *
         * @see Zone::Allocator, deleteBatch()
         *
         * @return  Pointer to the allocator. The zone will automatically delete the
         *      batch if its tag gets purged or when the zone is deleted.
         */
        template <typename Type>
        Allocator<Type>* newAllocator(duint batchSize, PurgeTag tag = STATIC) {
            _batches.push_back(new Allocator<Type>(*this, batchSize, tag));
            return static_cast<Allocator<Type>*>(_batches.back());
        }
        
        /**
         * Deletes a batch owned by the zone. Note that batches are freed automatically
         * according to the purge tags.
         *
         * @param batch  Batch to delete. @c delete is called on this.
         */
        void deleteBatch(Batch* batch);

    public:
        /**
         * An allocator utility that efficiently allocates a large
         * number of memory blocks of a specific type. It should be used in 
         * performance-critical places instead of separate calls to allocate().
         */
        class LIBDENG2_API Batch
        {
        public:
            /**
             * Constructs a new block memory allocator. These are used instead of many
             * calls to Zone::allocate() when the number of required elements is unknown and
             * when linear allocation would be too slow.
             *
             * Memory is allocated as needed in blocks of @a batchSize elements. When
             * a new element is required we simply reserve a pointer in the previously
             * allocated block of elements or create a new block just in time.
             *
             * @param zone  Memory zone for the allocations.
             * @param elementSize  Size of one element.
             * @param batchSize  Number of elements in each zblock of the set.
             * @param tag  Purge tag for the batch itself and the elements.
             *
             * @return  Pointer to the newly created batch.
             */
            Batch(Zone& zone, dsize elementSize, dsize batchSize, PurgeTag tag = STATIC) 
                : _zone(zone), _elementSize(elementSize), _elementsPerBlock(batchSize), 
                  _tag(tag), _max(0), _count(0), _blocks(0) {
                // The first block.
                expand();
            }

            /**
             * Frees the entire batch.
             * All memory allocated is released for all elements in all blocks.
             */
            virtual ~Batch() {
                if(_blocks) {
                    // Free the elements from each block.
                    for(dsize i = 0; i < _count; ++i) {
                        _zone.free(_blocks[i].elements);
                    }
                    _zone.free(_blocks);
                }
            }

            /**
             * Allocates a new element within the batch.
             *
             * @return  Pointer to memory block. Do not call Zone::free() on this.
             */
            virtual void* allocateElement() {
                // When this is called, there is always an available element in the topmost
                // block. We will return it.
                ZBlock* block = lastBlock();
                void* element = block->elements + _elementSize * block->count;

                // Reserve the element.
                block->count++;

                // If we run out of space in the topmost block, add a new one.
                if(block->count == block->max) {
                    expand();
                }
                return element;
            }
            
            /**
             * Returns the purge tag used by the allocator.
             */
            PurgeTag tag() const { return _tag; }
            
        protected:
            struct ZBlock {
                dsize max;              ///< maximum number of elements
                dsize count;            ///< number of used elements
                dbyte* elements;        ///< block of memory where elements are
            };

            /**
             * Returns the last block.
             */ 
            ZBlock* lastBlock() {
                Q_ASSERT(_count > 0);
                return &_blocks[_count - 1];
            }

            /**
             * Allocate new block(s) of memory to be used for linear object allocations.
             */
            void expand() {
                // Get a new block by resizing the blocks array. This is done relatively
                // seldom, since there is a large number of elements per each block.
                _count++;
                if(_count > _max || !_max) {
                    _max *= 2;
                    if(!_max) _max = _count;
                    _blocks = _zone.resizeClear<ZBlock>(_blocks, _max, _tag);
                }

                // Initialize the block's data.
                ZBlock* block = lastBlock();
                block->max = _elementsPerBlock;
                block->elements = _zone.allocate<dbyte>(_elementSize * block->max, _tag);
                block->count = 0;
            }

        private:
            Zone& _zone;

            const dsize _elementSize;

            dsize _elementsPerBlock;

            /// All blocks in a blockset have the same tag.
            PurgeTag _tag; 

            dsize _max;
            dsize _count;
            
            ZBlock* _blocks;
        };
        
        /**
         * Specialized batch allocator.
         */
        template <typename Type>
        class Allocator : public Batch
        {
        public:
            typedef Type Element;
            
            Allocator(Zone& zone, dsize batchSize, PurgeTag tag = STATIC) 
                : Batch(zone, sizeof(Type), batchSize, tag) {}
                
            Type* allocate() {
                return static_cast<Type*>(allocateElement());
            }
        };

    protected:
        struct MemZone;
        struct MemVolume;
        struct MemBlock;
        struct BlockSet;

        /**
         * Gets the memory block header of an allocated memory block.
         * 
         * @param ptr  Pointer to memory allocated from the zone.
         *
         * @return  Block header.
         */
        MemBlock* getBlock(void* ptr) const;
        
        /**
         * Create a new memory volume.  The new volume is added to the list of
         * memory volumes.
         *
         * @return  Pointer to the new volume.
         */
        MemVolume* newVolume(dsize volumeSize);

    private:
        MemVolume* _volumeRoot;

        /**
         * If false, alloc() will free purgable blocks and aggressively look for
         * free memory blocks inside each memory volume before creating new volumes.
         * This leads to slower mallocing performance, but reduces memory fragmentation
         * as free and purgable blocks are utilized within the volumes. Fast mode is
         * enabled during map setup because a large number of mallocs will occur
         * during setup.
         */
        bool _fastMalloc;
        
        typedef std::list<Batch*> Batches;
        Batches _batches;
    };
    
}

#endif /* LIBDENG2_ZONE_H */
