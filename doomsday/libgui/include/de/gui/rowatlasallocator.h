/** @file rowatlasallocator.h  Row-based atlas allocator.
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

#ifndef LIBGUI_ROWATLASALLOCATOR_H
#define LIBGUI_ROWATLASALLOCATOR_H

#include "../Atlas"

namespace de {

/**
 * Row-based atlas allocator.
 *
 * Suitable for content that uses relatively similar heights, for instance text
 * fragments/words.
 *
 * @see Atlas
 */
class LIBGUI_PUBLIC RowAtlasAllocator : public Atlas::IAllocator
{
public:
    RowAtlasAllocator();

    void setMetrics(Atlas::Size const &totalSize, int margin);

    void clear();
    Id allocate(Atlas::Size const &size, Rectanglei &rect);
    void release(Id const &id);
    bool optimize();

    int count() const;
    Atlas::Ids ids() const;
    void rect(Id const &id, Rectanglei &rect) const;
    Allocations allocs() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_ROWATLASALLOCATOR_H
