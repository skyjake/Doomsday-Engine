/** @file multiatlas.h  Collection of multiple atlases.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_MULTIATLAS_H
#define LIBGUI_MULTIATLAS_H

#include "de/atlas.h"
#include <de/asset.h>

namespace de {

/**
 * Automatically expanding collection of Atlas objects.
 *
 * Allocations are done using the MultiAtlas::AllocGroup utility. Each group
 * of allocations is guaranteed to reside on the same atlas, so that draw
 * operations can still bind to just one texture at a time.
 *
 * The user is expected to retain ownership of the MultiAtlas::AllocGroup
 * object for as long as the allocations are in use. If the AllocGroup is
 * destroyed, the related allocations are released from their atlas.
 *
 * After a set of allocations has been done in an AllocGroup, the group must
 * be manually committed as a whole before any of the images can be used.
 *
 * A new Atlas object is created if a group of allocations cannot be committed
 * to any of the existing ones. The creation of new Atlas objects is the
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC MultiAtlas
{
public:
    DE_ERROR(InvalidError);

    /**
     * Group of allocations.
     *
     * Asset status reflects whether all the images are committed and ready
     * for use.
     *
     * When deleted, releases all allocations.
     */
    class LIBGUI_PUBLIC AllocGroup : public IAtlas, public Asset
    {
    public:
        AllocGroup(MultiAtlas &multiAtlas);

        /**
         * Makes a new pending allocation.
         *
         * @param image    Image content.
         * @param knownId  Previously chosen Id for the image (if any).
         *
         * @return Id of the new allocation. This Id will be used for final
         * committed allocation, too.
         */
        Id alloc(const Image &image, const Id &knownId = Id::None) override;

        void release(const Id &id) override;

        bool contains(const Id &id) const override;

        /**
         * Commit all the allocated images.
         */
        void commit() const override;

        Rectanglef imageRectf(const Id &id) const override;

        /**
         * Returns the Atlas where the group's images have been allocated to.
         *
         * @return Atlas object (if asset ready), or nullptr if the AllocGroup
         * has not been committed yet (asset not ready).
         */
        const Atlas *atlas() const;

        /**
         * Returns the MultiAtlas this allocation group belongs to.
         */
        MultiAtlas &multiAtlas();

    private:
        DE_PRIVATE(d)
    };

    /// Interface for Atlas factories.
    class LIBGUI_PUBLIC IAtlasFactory
    {
    public:
        virtual ~IAtlasFactory() = default;

        /**
         * Creates a new Atlas for @a owner. Atlases used with MultiAtlas
         * must use the Atlas::DeferredAllocations mode.
         *
         * @param owner  MultiAtlas that will own the created Atlas. The
         *               factory may use this to check relevant information
         *               about what kind of atlas to create.
         * @return Atlas object.
         */
        virtual Atlas *makeAtlas(MultiAtlas &owner) = 0;
    };

public:
    MultiAtlas(IAtlasFactory &factory);

    /**
     * Destroys all the atlases. Existing AllocGroups will become invalid.
     */
    void clear();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_MULTIATLAS_H

