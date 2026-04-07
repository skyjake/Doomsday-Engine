/** @file directoryfeed.h  Directory Feed.
 *
 * @author Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_DIRECTORYFEED_H
#define LIBCORE_DIRECTORYFEED_H

#include "de/file.h"
#include "de/feed.h"
#include "de/nativepath.h"

namespace de {

/**
 * Reads from and writes to directories in the native file system.
 *
 * @ingroup fs
 */
class DE_PUBLIC DirectoryFeed : public Feed
{
public:
    /// The native directory was not found. @ingroup errors
    DE_ERROR(NotFoundError);

    /// Failed attempt to find out the status of a file. @ingroup errors
    DE_ERROR(StatusError);

    /// An error occurred changing the working directory. @ingroup errors
    DE_ERROR(WorkingDirError);

    enum Flag
    {
        /// Opens all files and folders in write mode.
        AllowWrite = 0x1,

        /// Creates the native directory if not does not exist.
        CreateIfMissing = 0x2,

        /// When populating the contents of the folder, descend to native
        /// subfolders.
        PopulateNativeSubfolders = 0x4,

        /// Create all files as native files without passing through any interpreters.
        DisableInterpreters = 0x8,

        OnlyThisFolder = 0,

        DefaultFlags = PopulateNativeSubfolders
    };

public:
    /**
     * Constructs a DirectoryFeed that accesses a directory in the native file system.
     *
     * @param nativePath  Path of the native directory.
     * @param mode        Feed mode.
     */
    DirectoryFeed(const NativePath &nativePath, const Flags &mode = DefaultFlags);

    /**
     * Sets the file name pattern that determines which files are populated by the feed.
     * By default, the pattern includes all files.
     *
     * @param namePattern  File name pattern.
     */
    void setNamePattern(const String &namePattern);

    String description() const;

    /**
     * Returns the native path of the source folder.
     */
    const NativePath &nativePath() const;

    PopulatedFiles populate(const Folder &folder);
    bool prune(File &file) const;
    File *createFile(const String &name);
    void destroyFile(const String &name);
    Feed *newSubFeed(const String &name);

public:
    /**
     * Changes the native working directory.
     *
     * @param nativePath  New path to use as the working directory.
     */
    static void changeWorkingDir(const NativePath &nativePath);

    /**
     * Determines the status of a file in the directory.
     * StatusError is thrown if the file status cannot be determined
     * due to any reason.
     *
     * @param nativePath  Path of the file.
     *
     * @return  Status of the file.
     */
    static File::Status fileStatus(const NativePath &nativePath);

    static void setFileModifiedTime(const NativePath &nativePath, const Time &modifiedAt);

    /**
     * Creates and interprets a single native file and adds it to a folder.
     *
     * The intended use of this method is to provide access to specific single native files
     * anywhere in the native file system. The caller must prepare an empty or reusable folder
     * where the new file object will be placed. A new DirectoryFeed will populate and prune
     * the file.
     *
     * @param nativePath    Native path of the source file.
     * @param parentFolder  Folder where to place the interpreted file. The existing contents
     *                      (both files and feeds) of this folder will be cleared beforehand.
     *
     * @return Reference to the interpreted file.
     */
    static File &manuallyPopulateSingleFile(const NativePath &nativePath, Folder &parentFolder);

protected:
    void populateSubFolder(const Folder &folder, const String &entryName);
    void populateFile(const Folder &folder, const String &entryName, PopulatedFiles &populated);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_DIRECTORYFEED_H */
