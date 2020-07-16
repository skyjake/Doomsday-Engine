/** @file fileindex.h  Index for looking up files of specific type.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_FILEINDEX_H
#define LIBCORE_FILEINDEX_H

#include "de/file.h"

#include <unordered_map>
#include <list>
#include <utility>

namespace de {

class File;
class Folder;
class Package;

/**
 * Indexes files for quick access.
 *
 * @ingroup fs
 */
class DE_PUBLIC FileIndex
{
public:
    typedef std::unordered_multimap<String, File *> Index; // lower-case
    typedef std::pair<Index::iterator, Index::iterator> IndexRange;
    typedef std::list<File *> FoundFiles;

    class DE_PUBLIC IPredicate
    {
    public:
        virtual ~IPredicate() = default;

        /**
         * Determines if a file should be included in the index.
         * @param file  File.
         *
         * @return @c true to index file, @c false to ignore.
         */
        virtual bool shouldIncludeInIndex(const File &file) const = 0;
    };

    DE_AUDIENCE(Addition, void fileAdded  (const File &, const FileIndex &))
    DE_AUDIENCE(Removal,  void fileRemoved(const File &, const FileIndex &))

public:
    FileIndex();

    /**
     * Sets the predicate that determines whether a file should be included in the
     * index.
     *
     * @param predicate  Predicate for inclusion. Must exist while the index is being
     *                   used.
     */
    void setPredicate(const IPredicate &predicate);

    /**
     * Adds a file to the index if the predicate permits.
     *
     * @param file  File.
     *
     * @return @c true, if the file was added to the index.
     */
    bool maybeAdd(const File &file);

    /**
     * Removes a file from the index, if it has been indexed. If not, nothing is done.
     *
     * @param file  File.
     */
    void remove(const File &file);

    int size() const;

    enum Behavior { FindInEntireIndex, FindOnlyInLoadedPackages };

    void findPartialPath(const String &path, FoundFiles &found,
                         Behavior behavior = FindInEntireIndex) const;

    /**
     * Finds partial paths that reside somewhere inside a specific folder
     * or one of its subfolders.
     *
     * @param rootFolder  Folder under which to confine the search.
     * @param path        Partial path to locate.
     * @param found       All matching files.
     * @param behavior    Search behavior.
     */
    void findPartialPath(const Folder &rootFolder,
                         const String &path,
                         FoundFiles &found,
                         Behavior behavior = FindInEntireIndex) const;

    /**
     * Finds partial paths that reside in a specific package.
     *
     * @param packageId  Package whose contents to search.
     * @param path       Partial path to find.
     * @param found      All matching files.
     */
    void findPartialPath(const String &packageId,
                         const String &path,
                         FoundFiles &found) const;

    /**
     * Finds all instances of a (partial) path within the index. The results are sorted
     * so that they are in the same order as the packages in which the files are located.
     * Files that are not in any package appear before files that belong to a package
     * (when using FindInEntireIndex behavior).
     *
     * If package A has been loaded before package B, files from A are sorted before B
     * in the resulting list of files.
     *
     * @param path      Partial path to find.
     * @param found     Found files.
     * @param behavior  Behavior for finding: which results to filter out.
     *
     * @return  Number of files found.
     */
    int findPartialPathInPackageOrder(const String &path, FoundFiles &found,
                                      Behavior behavior = FindOnlyInLoadedPackages) const;

    void print() const;

    List<File *> files() const;

protected:
    // C++ iterator (not thread-safe):
    typedef Index::const_iterator const_iterator;
    Index::const_iterator begin() const;
    Index::const_iterator end() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_FILEINDEX_H
