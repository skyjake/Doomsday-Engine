/**\file lumpinfo.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_LUMPINFO_H
#define LIBDENG_FILESYS_LUMPINFO_H

#include <de/types.h>

#ifdef __cplusplus
#include <algorithm>

extern "C" {
#endif

struct abstractfile_s;

/**
 * LumpInfo record. POD.
 * @ingroup fs
 */
typedef struct lumpinfo_s {
    uint lastModified; /// Unix timestamp.
    int lumpIdx; /// Relative index of this lump in the owning package else zero.
    size_t baseOffset; /// Offset from the start of the owning package.
    size_t size; /// Size of the uncompressed file.
    size_t compressedSize; /// Size of the original file compressed.

    /// @todo Move this property up to file level.
    struct abstractfile_s* container; /// Owning package else @c NULL.

#ifdef __cplusplus
    lumpinfo_s(uint _lastModified = 0, int _lumpIdx = 0, size_t _baseOffset = 0,
               size_t _size = 0, size_t _compressedSize = 0, struct abstractfile_s* _container = 0)
        : lastModified(_lastModified), lumpIdx(_lumpIdx), baseOffset(_baseOffset),
          size(_size), compressedSize(_compressedSize), container(_container)
    {}

    lumpinfo_s(lumpinfo_s const& other)
        : lastModified(other.lastModified), lumpIdx(other.lumpIdx), baseOffset(other.baseOffset),
          size(other.size), compressedSize(other.compressedSize), container(other.container)
    {}

    ~lumpinfo_s() {}

    lumpinfo_s& operator = (lumpinfo_s other)
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(lumpinfo_s& first, lumpinfo_s& second) // nothrow
    {
        using std::swap;
        swap(first.lastModified,    second.lastModified);
        swap(first.lumpIdx,         second.lumpIdx);
        swap(first.baseOffset,      second.baseOffset);
        swap(first.size,            second.size);
        swap(first.compressedSize,  second.compressedSize);
        swap(first.container,       second.container);
    }

    inline bool isCompressed() const { return size != compressedSize; }
#endif
} LumpInfo;

void F_InitLumpInfo(LumpInfo* info);
void F_CopyLumpInfo(LumpInfo* dst, const LumpInfo* src);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_LUMPINFO_H */
