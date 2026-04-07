/** @file atlas.h  Image-based atlas.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBGUI_ATLAS_H
#define LIBGUI_ATLAS_H

#include <de/deletable.h>
#include <de/hash.h>
#include <de/id.h>
#include <de/lockable.h>
#include <de/observers.h>
#include <de/rectangle.h>
#include <de/set.h>

#include "de/image.h"

namespace de {

/**
 * Interface for any Atlas-type class. @ingroup gl
 *
 * The allocations must be committed before being used.
 */
class LIBGUI_PUBLIC IAtlas
{
public:
    DE_ERROR(OutOfSpaceError);

    virtual ~IAtlas() = default;

    virtual Id alloc(const Image &image, const Id &chosenId = Id::None) = 0;
    virtual void release(const Id &id) = 0;
    virtual bool contains(const Id &id) const = 0;
    virtual void commit() const = 0;

    virtual Rectanglef imageRectf(const Id &id) const = 0;
};

/**
 * Abstract image-based atlas.
 *
 * The logic that determines how and where new content is allocated is
 * completely handled by the IAllocator attached to the Atlas.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC Atlas : public IAtlas, public Lockable, public Deletable
{
public:
    typedef Image::Size Size;

    enum Flag
    {
        /**
         * A copy of the full atlas is kept in memory.
         */
        BackingStore = 0x1,

        /**
         * When the atlas is too full, it will be defragmented in an attempt to
         * rearrange the content more efficiently. Useful with dynamic atlases
         * where lots of allocations and releases occur predictably. Requires
         * BackingStore.
         */
        AllowDefragment = 0x2,

        /**
         * If using a backing store, wrap borders using the source image. This allows
         * filtering the contents using wrapped coordinates. Borders are by default
         * duplicated from neighboring pixels (for clamped filtering). Set border size
         * with setBorderSize().
         */
        WrapBordersInBackingStore = 0x4,

        /**
         * All commits are logged as XVerbose log entries. A commit occurs when the
         * atlas backing store contents are copied to the actual atlas storage (for
         * instance a GL texture).
         */
        LogCommitsAsXVerbose = 0x8,

        /**
         * Allocations will not be committed until manually requested. Deferred
         * allocations can also be cancelled before committing.
         */
        DeferredAllocations = 0x10,

        DefaultFlags = 0
    };

    typedef Set<Id> Ids;

    /**
     * Interface for allocator logic. Each Atlas requires one IAllocator object to
     * determine where to place allocated images.
     */
    class LIBGUI_PUBLIC IAllocator
    {
    public:
        typedef Hash<Id, Rectanglei> Allocations;

    public:
        virtual ~IAllocator() = default;

        /**
         * Define the metrics for the atlas.
         *
         * @param totalSize  Total area available.
         * @param margin     Number of pixels to leave between each allocated
         *                   image and also each edge of the total area.
         */
        virtual void setMetrics(const Size &totalSize, int margin) = 0;

        virtual void clear() = 0;
        virtual Id   allocate(const Size &size, Rectanglei &rect, const Id &knownId) = 0;
        virtual void release(const Id &id) = 0;

        /**
         * Finds an optimal layout for all of the allocations.
         */
        virtual bool optimize() = 0;

        virtual int  count() const = 0;
        virtual Ids  ids() const = 0;
        virtual void rect(const Id &id, Rectanglei &rect) const = 0;

        /**
         * Returns all the present allocations.
         */
        virtual Allocations allocs() const = 0;
    };

    /**
     * Audience that will be notified if the existing allocations are
     * repositioned for some reasons (e.g., defragmentation). Normally once
     * allocated, content will remain at its initial place.
     */
    DE_AUDIENCE(Reposition, void atlasContentRepositioned(Atlas &))

    /**
     * Audience that will be notified when an allocation fails due to the atlas
     * being so full that there is no room for the new image.
     */
    DE_AUDIENCE(OutOfSpace, void atlasOutOfSpace(Atlas &))

public:
    /**
     * Constructs a new atlas.
     */
    Atlas(const Flags &flags = DefaultFlags, const Size &totalSize = Size());

    Flags flags() const;

    /**
     * Sets the allocator for the atlas. The atlas is cleared automatically.
     *
     * @param allocator  Allocator instance. Atlas gets ownership.
     */
    void setAllocator(IAllocator *allocator);

    IAllocator *takeAllocator();

    /**
     * Sets the size of the margin that is left between allocations. The default is
     * one (transparent black) pixel.
     *
     * @param marginPixels  Number of pixels to retain as margins around allocations.
     */
    void setMarginSize(dint marginPixels);

    /**
     * Sets the size of borders that are added around allocations. The size of the
     * allocated area is increased internally by this many units on each size.
     *
     * If WrapBordersInBackingStore is used, each border is taken from the opposite
     * side of the source image. By default, the borders duplicate the neighboring
     * pixels on each edge.
     *
     * @param borderPixels  Number of pixels to add as border padding.
     */
    void setBorderSize(dint borderPixels);

    /**
     * Empties the contents of the atlas. The size of the backing store is not
     * changed.
     */
    void clear();

    /**
     * Resizes the atlas.
     *
     * @param totalSize  Total size of the atlas in pixels.
     */
    void setTotalSize(const Size &totalSize);

    Size totalSize() const;

    /**
     * Attempts to allocate an image into the atlas. If defragmentation is
     * allowed, it may occur during the operation.
     *
     * @param image     Image content to allocate.
     * @param chosenId  Id for the allocation, if it has already been chosen.
     *
     * @return Identifier of the allocated image. If Id::None, the allocation
     * failed because the atlas is too full.
     */
    Id alloc(const Image &image, const Id &chosenId = Id::None) override;

    /**
     * Releases a previously allocated image from the atlas.
     *
     * @param id  Identifier of an allocated image.
     */
    void release(const Id &id) override;

    bool contains(const Id &id) const override;

    /**
     * Request committing the backing store to the physical atlas storage.
     * This does nothing if there are no changes in the atlas.
     *
     * Deferred commits will also be committed.
     */
    void commit() const override;

    /**
     * Deferred allocations will all be cancelled.
     */
    void cancelDeferred();

    /**
     * Returns the number of images in the atlas.
     */
    int imageCount() const;

    inline bool isEmpty() const { return !imageCount(); }

    /**
     * Returns the identifiers of all images in the atlas.
     */
    Ids allImages() const;

    /**
     * Returns the position of an allocated image in the atlas.
     *
     * @param id  Image identifier.
     *
     * @return Coordinates of the image on the atlas (as pixels). Always within
     * the Atlas::size().
     */
    Rectanglei imageRect(const Id &id) const;

    /**
     * Returns the normalized position of an allocated image in the atlas.
     *
     * @param id  Image identifier.
     *
     * @return Normalized coordinates of the image on the atlas. Always within
     * [0,1].
     */
    Rectanglef imageRectf(const Id &id) const override;

    /**
     * Returns the image content allocated earlier. Requires BackingStore.
     *
     * @param id  Image identifier.
     *
     * @return Image that was provided earlier to alloc().
     */
    Image image(const Id &id) const;

protected:
    virtual void commitFull(const Image &fullImage) const = 0;

    /**
     * Commits an an image to the actual physical atlas storage.
     *
     * @param image    Image to commit.
     * @param topLeft  Top left corner of where to place the image.
     */
    virtual void commit(const Image &image, const Vec2i &topLeft) const = 0;

    /**
     * Commits a subregion of an image to the actual physical atlas storage.
     *
     * @param fullImage  Image to commit.
     * @param subregion  Section of the image to commit.
     */
    virtual void commit(const Image &fullImage, const Rectanglei &subregion) const = 0;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_ATLAS_H
