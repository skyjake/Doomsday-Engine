/**
 * @file fileinfo.h
 *
 * Metadata (record) for a file in the engine's virtual file system.
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_FILEINFO_H
#define LIBDENG_FILESYS_FILEINFO_H

#include <algorithm>

#include <de/types.h>
#include "abstractfile.h"

namespace de {

/**
 * FileInfo record.
 * @ingroup fs
 */
struct FileInfo
{
    uint lastModified; /// Unix timestamp.
    int lumpIdx; /// Relative index of this lump in the owning package else zero.
    size_t baseOffset; /// Offset from the start of the owning package.
    size_t size; /// Size of the uncompressed file.
    size_t compressedSize; /// Size of the original file compressed.

    /// @todo Move this property up to file level.
    AbstractFile* container; /// Owning package else @c NULL.

    FileInfo(uint _lastModified = 0, int _lumpIdx = 0, size_t _baseOffset = 0,
               size_t _size = 0, size_t _compressedSize = 0, AbstractFile* _container = 0)
        : lastModified(_lastModified), lumpIdx(_lumpIdx), baseOffset(_baseOffset),
          size(_size), compressedSize(_compressedSize), container(_container)
    {}

    FileInfo(FileInfo const& other)
        : lastModified(other.lastModified), lumpIdx(other.lumpIdx), baseOffset(other.baseOffset),
          size(other.size), compressedSize(other.compressedSize), container(other.container)
    {}

    ~FileInfo() {}

    FileInfo& operator = (FileInfo other)
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(FileInfo& first, FileInfo& second) // nothrow
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
};

} // namespace de

#endif /* LIBDENG_FILESYS_FILEINFO_H */
