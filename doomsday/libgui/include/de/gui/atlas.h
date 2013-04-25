/** @file atlas.h  Image-based atlas.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_ATLAS_H
#define LIBGUI_ATLAS_H

#include <QFlags>
#include <QSet>
#include <QMap>
#include <de/Id>
#include <de/Rectangle>
#include <de/Observers>

#include "../Image"

namespace de {

/**
 * Abstract image-based atlas.
 *
 * The logic that determines how and where new content is allocated is
 * completely handled by the IAllocator attached to the Atlas.
 *
 * @ingroup gui
 */
class LIBGUI_PUBLIC Atlas
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

        DefaultFlags = 0
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    typedef QSet<Id> Ids;

    /**
     * Interface for allocator logic.
     */
    class LIBGUI_PUBLIC IAllocator
    {
    public:
        typedef QMap<Id, Rectanglei> Allocations;

    public:
        virtual ~IAllocator() {}

        /**
         * Define the metrics for the atlas.
         *
         * @param totalSize  Total area available.
         * @param margin     Number of pixels to leave between each allocated
         *                   image and also each edge of the total area.
         */
        virtual void setMetrics(Size const &totalSize, int margin) = 0;

        virtual void clear() = 0;
        virtual Id   allocate(Size const &size, Rectanglei &rect) = 0;
        virtual void release(Id const &id) = 0;

        /**
         * Finds an optimal layout for all of the allocations.
         */
        virtual bool optimize() = 0;

        virtual int  count() const = 0;
        virtual Ids  ids() const = 0;
        virtual void rect(Id const &id, Rectanglei &rect) const = 0;

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
    DENG2_DEFINE_AUDIENCE(Reposition, void atlasContentRepositioned(Atlas &))

public:
    /**
     * Constructs a new atlas.
     */
    Atlas(Flags const &flags = DefaultFlags, Size const &totalSize = Image::Size());

    /**
     * Sets the allocator for the atlas. The atlas is cleared automatically.
     *
     * @param allocator  Allocator instance. Atlas gets ownership.
     */
    void setAllocator(IAllocator *allocator);

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
    void setTotalSize(Size const &totalSize);

    Size totalSize() const;

    /**
     * Attempts to allocate an image into the atlas. If defragmentation is
     * allowed, it may occur during the operation.
     *
     * @param image  Image content to allocate.
     *
     * @return Identifier of the allocated image. If Id::None, the allocation
     * failed because the atlas is too full.
     */
    Id alloc(Image const &image);

    /**
     * Releases a previously allocated image from the atlas.
     *
     * @param id  Identifier of an allocated image.
     */
    void release(Id const &id);

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
    Rectanglei imageRect(Id const &id) const;

    /**
     * Returns the normalized position of an allocated image in the atlas.
     *
     * @param id  Image identifier.
     *
     * @return Normalized coordinates of the image on the atlas. Always within
     * [0,1].
     */
    Rectanglef imageRectf(Id const &id) const;

    /**
     * Request committing the backing store to the physical atlas storage.
     * This does nothing if there are no changes in the atlas.
     */
    void commit() const;

protected:
    virtual void commitFull(Image const &fullImage) const = 0;

    /**
     * Commits a image to the actual physical atlas storage.
     *
     * @param image    Image to commit.
     * @param topLeft  Top left corner of where to place the image.
     */
    virtual void commit(Image const &image, Vector2i const &topLeft) const = 0;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_ATLAS_H
