/** @file lumpindex.h  Index of lumps.
 *
 * @todo Move the definition of lumpnum_t into this header.
 *
 * @author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_FILESYS_LUMPINDEX_H
#define DE_FILESYS_LUMPINDEX_H

#ifdef __cplusplus

#include "../libdoomsday.h"
#include "file.h"
#include "fileinfo.h"

#include <list>
#include <de/list.h>
#include <de/keymap.h>
#include <de/error.h>

namespace res {

using namespace de;

/**
 * Virtual file system component used to model an indexable collection of
 * lumps. A single index may include lumps originating from many different
 * file containers.
 *
 * @ingroup fs
 */
class LIBDOOMSDAY_PUBLIC LumpIndex
{
public:
    /// No file(s) found. @ingroup errors
    DE_ERROR(NotFoundError);

    typedef List<File1 *> Lumps;
    typedef std::list<lumpnum_t> FoundIndices;

    /**
     * Heuristic based map data (format) recognizer.
     *
     * Unfortunately id Tech 1 maps cannot be easily recognized, due to their
     * lack of identification signature, the mechanics of the WAD format lump
     * index and the existence of several subformat variations. Therefore it is
     * necessary to use heuristic analysis of the lump index and the lump data.
     */
    class LIBDOOMSDAY_PUBLIC Id1MapRecognizer
    {
    public:
        /// Logical map format identifiers.
        enum Format {
            UnknownFormat = -1,

            DoomFormat,
            HexenFormat,
            Doom64Format,
            UniversalFormat, // UDMF

            KnownFormatCount
        };

        /// Logical map data type identifiers:
        enum DataType {
            UnknownData = -1,

            ThingData,
            LineDefData,
            SideDefData,
            VertexData,
            SegData,
            SubsectorData,
            NodeData,
            SectorDefData,
            RejectData,
            BlockmapData,
            BehaviorData,
            ScriptData,
            TintColorData,
            MacroData,
            LeafData,
            GLVertexData,
            GLSegData,
            GLSubsectorData,
            GLNodeData,
            GLPVSData,
            UDMFTextmapData,
            UDMFEndmapData,

            KnownDataCount
        };

        typedef KeyMap<DataType, File1 *> Lumps;

    public:
        /**
         * Attempt to recognize an id Tech 1 format by traversing the WAD lump
         * index, beginning at the @a lumpIndexOffset specified.
         */
        Id1MapRecognizer(const LumpIndex &lumpIndex, lumpnum_t lumpIndexOffset);

        const String &id() const;
        Format format() const;

        const Lumps &lumps() const;

        File1 *sourceFile() const;

        /**
         * Returns the lump index number of the last data lump inspected by the
         * recognizer, making it possible to collate/locate all the map data sets
         * using multiple recognizers.
         */
        lumpnum_t lastLump() const;

    public:
        /**
         * Returns the textual name for the identified map format @a id.
         */
        static const String &formatName(Format id);

        /**
         * Determines the type of a map data lump by @a name.
         */
        static DataType typeForLumpName(String name);

        /**
         * Determine the size (in bytes) of an element of the specified map data
         * lump @a type for the current map format.
         *
         * @param mapFormat     Map format identifier.
         * @param dataType      Map lump data type.
         *
         * @return Size of an element of the specified type.
         */
        static dsize elementSizeForDataType(Format mapFormat, DataType dataType);

    private:
        DE_PRIVATE(d)
    };

public:
    /**
     * @param pathsAreUnique  Lumps in the index must have unique paths. Inserting
     *                        a lump with the same path as one which already exists
     *                        will result in the earlier lump being pruned.
     */
    explicit LumpIndex(bool pathsAreUnique = false);
    virtual ~LumpIndex();

    /**
     * Returns @c true iff the directory contains no lumps.
     */
    inline bool isEmpty() const { return !size(); }

    /**
     * Returns the total number of lumps in the directory.
     */
    int size() const;

    /// @copydoc size()
    inline int lumpCount() const { return size(); } // alias

    /**
     * Returns the logicial index of the last lump in the directory, or @c -1 if empty.
     */
    int lastIndex() const;

    /**
     * Returns @c true iff @a lumpNum can be interpreted as a valid lump index.
     */
    bool hasLump(lumpnum_t lumpNum) const;

    /**
     * Returns @c true iff the index contains one or more lumps with a matching @a path.
     */
    bool contains(const Path &path) const;

    /**
     * Finds all indices for lumps with a matching @a path.
     *
     * @param path   Path of the lump(s) to .
     * @param found  Set of lumps that match @a path (in load order; most recent last).
     *
     * @return  Number of lumps found.
     *
     * @see findFirst(), findLast()
     */
    int findAll(const Path &path, FoundIndices &found) const;

    /**
     * Returns the index of the @em first loaded lump with a matching @a path.
     * If no lump is found then @c -1 is returned.
     *
     * @see findLast(), findAll()
     */
    lumpnum_t findFirst(const Path &path) const;

    /**
     * Returns the index of the @em last loaded lump with a matching @a path.
     * If no lump is found then @c -1 is returned.
     *
     * @see findFirst(), findAll()
     */
    lumpnum_t findLast(const Path &path) const;

    /**
     * Lookup a file at specific offset in the index.
     *
     * @param lumpNum  Logical lumpnum associated to the file being looked up.
     *
     * @return  The requested file.
     *
     * @throws NotFoundError If the requested file could not be found.
     * @see hasLump()
     */
    File1 &lump(lumpnum_t lumpNum) const;

    /**
     * @copydoc lump()
     * @see lump()
     */
    inline File1 &operator [] (lumpnum_t lumpNum) const { return lump(lumpNum); }

    /**
     * Provides access to list containing @em all the lumps, for efficient traversals.
     */
    const Lumps &allLumps() const;

    /**
     * Clear the index back to its default (i.e., empty state).
     */
    void clear();

    /**
     * Are any lumps from @a file published in this index?
     *
     * @param file  File containing the lumps to look for.
     *
     * @return  @c true= One or more lumps are included.
     */
    bool catalogues(File1 &file);

    /**
     * Append a lump to the index.
     *
     * @post Lump name hashes may be invalidated (will be rebuilt upon next search).
     *
     * @param lump  Lump to be being added.
     */
    void catalogLump(File1 &lump);

    /**
     * Prune all lumps catalogued from @a file.
     *
     * @param file  File containing the lumps to prune
     *
     * @return  Number of lumps pruned.
     */
    int pruneByFile(File1 &file);

    /**
     * Prune the lump referenced by @a lumpInfo.
     *
     * @param lump  Lump file to prune.
     *
     * @return  @c true if found and pruned.
     */
    bool pruneLump(File1 &lump);

public:
    /**
     * Compose the path to the data resource.
     *
     * @param lumpNum  Lump number.
     *
     * @note We do not use the lump name, instead we use the logical lump index
     * in the global LumpIndex. This is necessary because of the way id tech 1
     * manages graphic references in animations (intermediate frames are chosen
     * by their 'original indices' rather than by name).
     */
    static res::Uri composeResourceUrn(lumpnum_t lumpNum);

private:
    DE_PRIVATE(d)
};

typedef LumpIndex::Id1MapRecognizer Id1MapRecognizer;

} // namespace res

#endif // __cplusplus
#endif // DE_FILESYS_LUMPINDEX_H
