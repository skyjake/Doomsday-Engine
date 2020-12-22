/** @file rowatlasallocator.h  Row-based atlas allocator.
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

#ifndef LIBGUI_ROWATLASALLOCATOR_H
#define LIBGUI_ROWATLASALLOCATOR_H

#include "de/atlas.h"

namespace de {

/**
 * Row-based atlas allocator.
 *
 * Suitable for content that uses relatively similar heights, for instance text
 * fragments/words.
 *
 * @see Atlas
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC RowAtlasAllocator : public Atlas::IAllocator
{
public:
    RowAtlasAllocator();

    void setMetrics(const Atlas::Size &totalSize, int margin) override;

    void clear() override;
    Id   allocate(const Atlas::Size &size, Rectanglei &rect, const Id &knownId) override;
    void release(const Id &id) override;
    bool optimize() override;

    int         count() const override;
    Atlas::Ids  ids() const override;
    void        rect(const Id &id, Rectanglei &rect) const override;
    Allocations allocs() const override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_ROWATLASALLOCATOR_H
