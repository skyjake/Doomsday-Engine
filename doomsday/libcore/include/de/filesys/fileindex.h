/** @file fileindex.h  Index for looking up files of specific type.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_FILEINDEX_H
#define LIBDENG2_FILEINDEX_H

#include "../File"

#include <map>
#include <list>
#include <utility>

namespace de {

class File;

/**
 * Indexes files for quick access.
 *
 * @ingroup fs
 */
class FileIndex
{
public:
    typedef std::multimap<String, File *> Index;
    typedef std::pair<Index::iterator, Index::iterator> IndexRange;
    typedef std::pair<Index::const_iterator, Index::const_iterator> ConstIndexRange;
    typedef std::list<File *> FoundFiles;

    class IPredicate
    {
    public:
        virtual ~IPredicate() {}

        /**
         * Determines if a file should be included in the index.
         * @param file  File.
         *
         * @return @c true to index file, @c false to ignore.
         */
        virtual bool shouldIncludeInIndex(File const &file) const = 0;
    };

    DENG2_DEFINE_AUDIENCE2(Addition, void fileAdded  (File const &, FileIndex &))
    DENG2_DEFINE_AUDIENCE2(Removal,  void fileRemoved(File const &, FileIndex &))

public:
    FileIndex();

    /**
     * Sets the predicate that determines whether a file should be included in the
     * index.
     *
     * @param predicate  Predicate for inclusion. Must exist while the index is being
     *                   used.
     */
    void setPredicate(IPredicate const &predicate);

    /**
     * Adds a file to the index if the predicate permits.
     *
     * @param file  File.
     *
     * @return @c true, if the file was added to the index.
     */
    bool maybeAdd(File const &file);

    /**
     * Removes a file from the index, if it has been indexed. If not, nothing is done.
     *
     * @param file  File.
     */
    void remove(File const &file);

    int size() const;

    void findPartialPath(String const &path, FoundFiles &found) const;

    void print() const;

    // C++ iterator:
    typedef Index::const_iterator const_iterator;
    Index::const_iterator begin() const;
    Index::const_iterator end() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_FILEINDEX_H
