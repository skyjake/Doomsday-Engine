/**
 * @file lumpindex.h
 *
 * Index of lumps.
 *
 * @ingroup fs
 *
 * Virtual file system component used to model an indexable collection of
 * lumps. A single index may include lumps originating from many different
 * file containers.
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

#ifndef LIBDENG_FILESYS_LUMPINDEX_H
#define LIBDENG_FILESYS_LUMPINDEX_H

#include "de_base.h" /// For lumpnum_t (@todo which should be moved here)
#include "filesys/file.h"
#include "filesys/fileinfo.h"

#include <QList>
#include <de/Error>

namespace de {

/**
 * @defgroup lumpIndexFlags Lump Index Flags
 * @ingroup flags
 * @{
 */

/// Lumps in the index must have unique paths. Inserting a lump with the same
/// path as one which already exists will result in the earlier lump being pruned.
#define LIF_UNIQUE_PATHS                0x1

/// @}

class LumpIndex
{
public:
    /// No file(s) found. @ingroup errors
    DENG2_ERROR(NotFoundError);

    typedef QList<File1*> Lumps;

public:
    /**
     * @param flags  @ref lumpIndexFlags
     */
    LumpIndex(int flags = 0);
    ~LumpIndex();

    /// Number of lumps in the directory.
    int size() const;

    /// @return  @c true iff @a lumpNum can be interpreted as a valid lump index.
    bool isValidIndex(lumpnum_t lumpNum) const;

    /// @return  Index associated with the last lump with variable-length @a path if found else @c -1
    lumpnum_t indexForPath(char const* path);

    /**
     * Lookup a file at specific offset in the index.
     *
     * @param lumpNum   Logical lumpnum associated to the file being looked up.
     *
     * @return  The requested file.
     *
     * @throws NotFoundError If the requested file could not be found.
     */
    File1& lump(lumpnum_t lumpNum) const;

    /**
     * Provides access to the list of lumps for efficient traversals.
     */
    Lumps const& lumps() const;

    /**
     * Clear the index back to its default (i.e., empty state).
     */
    void clear();

    /**
     * Are any lumps from @a file published in this index?
     *
     * @param file      File containing the lumps to look for.
     *
     * @return  @c true= One or more lumps are included.
     */
    bool catalogues(File1& file);

    /**
     * Append a new set of lumps to the index.
     *
     * @post Lump name hashes may be invalidated (will be rebuilt upon next search).
     *
     * @param file          File from which lumps are to be being added.
     * @param lumpIdxBase   Base index for the range of lumps being added.
     * @param lumpIdxCount  Number of lumps in the range being added.
     */
    void catalogLumps(File1& file, int lumpIdxBase, int lumpIdxCount);

    /**
     * Prune all lumps catalogued from @a file.
     *
     * @param file      File containing the lumps to prune
     *
     * @return  Number of lumps pruned.
     */
    int pruneByFile(File1& file);

    /**
     * Prune the lump referenced by @a lumpInfo.
     *
     * @param lump      Lump file to prune.
     *
     * @return  @c true if found and pruned.
     */
    bool pruneLump(File1& lump);

    /**
     * Print contents of index @a index.
     */
    static void print(LumpIndex const& index);

private:
    struct Instance;
    Instance* d;
};

} // namespace de

#endif // LIBDENG_FILESYS_LUMPINDEX_H
