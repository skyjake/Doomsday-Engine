/**
 * @file lumpfileadaptor.h
 *
 * Adaptor of File for working with the lumps of containers (such as Wad and
 * Zip) as if they were "real" files.
 *
 * @ingroup fs
 *
 * @see file.h, File
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_LUMPFILEADAPTOR_H
#define LIBDENG_FILESYS_LUMPFILEADAPTOR_H

#ifdef __cplusplus

#include "file.h"
#include "fileinfo.h"
#include "wad.h"
#include "zip.h"

namespace de {

class FileHandle;
class LumpIndex;
class PathDirectoryNode;

/**
 * LumpFileAdaptor. File adaptor class allowing lumps to be interfaced with as
 * if they were "real" files.
 */
class LumpFileAdaptor : public File1
{
public:
    LumpFileAdaptor(FileHandle& hndl, char const* path, FileInfo const& info,
                    File1* container = 0);
    ~LumpFileAdaptor();

    /// @return  Name of this file.
    ddstring_t const* name() const
    {
        if(Wad* wad = dynamic_cast<Wad*>(&container()))
        {
            return wad->lump(info().lumpIdx).name();
        }
        if(Zip* zip = dynamic_cast<Zip*>(&container()))
        {
            return zip->lump(info().lumpIdx).name();
        }
        throw de::Error("LumpFileAdaptor::name", "Unknown de::File1 type");
    }

    /**
     * Retrieve the directory node for this file.
     *
     * @return  Directory node for this file.
     */
    PathDirectoryNode const& directoryNode() const
    {
        if(Wad* wad = dynamic_cast<Wad*>(&container()))
        {
            return wad->lump(info().lumpIdx).directoryNode();
        }
        if(Zip* zip = dynamic_cast<Zip*>(&container()))
        {
            return zip->lump(info().lumpIdx).directoryNode();
        }
        throw de::Error("LumpFileAdaptor::directoryNode", "Unknown de::File1 type");
    }

    /**
     * Compose the absolute VFS path to a lump contained by this file.
     *
     * @note Always returns a valid string object. If @a lumpIdx is not valid a
     *       zero-length string is returned.
     *
     * @param lumpIdx       Logical index for the lump.
     * @param delimiter     Delimit directory separators using this character.
     *
     * @return String containing the absolute path.
     */
    AutoStr* composeLumpPath(int lumpIdx, char delimiter = '/');

    /**
     * Read the data associated with lump @a lumpIdx into @a buffer.
     *
     * @param lumpIdx       Lump index associated with the data to be read.
     * @param buffer        Buffer to read into. Must be at least large enough to
     *                      contain the whole lump.
     * @param tryCache      @c true= try the lump cache first.
     *
     * @return Number of bytes read.
     *
     * @see lumpSize() or lumpInfo() to determine the size of buffer needed.
     */
    size_t readLump(int lumpIdx, uint8_t* buffer, bool tryCache = true);

    /**
     * Read a subsection of the data associated with lump @a lumpIdx into @a buffer.
     *
     * @param lumpIdx       Lump index associated with the data to be read.
     * @param buffer        Buffer to read into. Must be at least @a length bytes.
     * @param startOffset   Offset from the beginning of the lump to start reading.
     * @param length        Number of bytes to read.
     * @param tryCache      @c true= try the lump cache first.
     *
     * @return Number of bytes read.
     */
    size_t readLump(int lumpIdx, uint8_t* buffer, size_t startOffset, size_t length,
                    bool tryCache = true);

    /**
     * Read the data associated with lump @a lumpIdx into the cache.
     *
     * @param lumpIdx   Lump index associated with the data to be cached.
     *
     * @return Pointer to the cached copy of the associated data.
     */
    uint8_t const* cacheLump(int lumpIdx);

    /**
     * Remove a lock on a cached data lump.
     *
     * @param lumpIdx   Lump index associated with the cached data to be changed.
     *
     * @return This instance.
     */
    LumpFileAdaptor& unlockLump(int lumpIdx);

private:
    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif // __cplusplus

struct lumpfileadaptor_s; // The lumpfileadaptor instance (opaque)
//typedef struct lumpfileadaptor_s LumpFileAdaptor;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_LUMPFILEADAPTOR_H */
