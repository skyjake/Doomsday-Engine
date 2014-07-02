/** @file package.h  Package containing metadata, data, and/or files.
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

#ifndef LIBDENG2_PACKAGE_H
#define LIBDENG2_PACKAGE_H

#include "../String"
#include "../File"

namespace de {

class Package;

#if 0
/**
 * Interface for anything that can contain a Package instance.
 */
class IPackageContainer
{
public:
    virtual ~IPackageContainer() {}

    virtual Package &package() = 0;

    virtual Package const &package() const {
        return const_cast<IPackageContainer *>(this)->package();
    }
};
#endif

/**
 * Container package with metadata, data, and/or files.
 * @ingroup fs
 *
 * A @em package is a collection of files packaged into a single unit (possibly using an
 * Archive). Examples of packages are add-on packages (in various formats, e.g., PK3/ZIP
 * archive or the Snowberry add-on bundle), savegames, custom maps, and demos.
 *
 * An instance of Package represents a package that is currently loaded. Note that
 * the package's metadata namespace is owned by the file that contains the package;
 * Package only consists of state that is relevant while the package is loaded (i.e.,
 * in active use).
 */
class Package
{
public:
    /// Package's source is missing or inaccessible. @ingroup errors
    DENG2_ERROR(SourceError);

    /// Package is missing some required metadata. @ingroup errors
    DENG2_ERROR(IncompleteMetadataError);

public:
    /**
     * Creates a package whose data comes from a file. The file's metadata is used
     * as the package's metadata namespace.
     *
     * @param file  File or folder containing the package's contents.
     */
    Package(File const &file);

    virtual ~Package();

    File const &file() const;

    /**
     * Returns the package's root folder, if it has one. Returns @c NULL if the package
     * is "flat" and comes with no folder structure.
     */
    Folder const *root() const;

    Record &info();

    Record const &info() const;

    /**
     * Returns the unique package identifier. This is the file name of the package
     * without any file extension.
     */
    String identifier() const;

    /**
     * Executes a script function in the metadata of the package.
     *
     * @param name  Name of the function to call.
     *
     * @return @c true, if the function exists and was called. @c false, if the
     * function was not found.
     */
    bool executeFunction(String const &name);

    /**
     * Called by PackageLoader after the package has been marked as loaded.
     */
    virtual void didLoad();

    /**
     * Called by PackageLoader immediately before the package is marked as unloaded.
     */
    virtual void aboutToUnload();

public:
    /**
     * Checks that all the metadata seems legit. An IncompleteMetadataError or
     * another exception is thrown if the package is not deemed valid.
     *
     * @param packageInfo  Metadata to validate.
     */
    static void validateMetadata(Record const &packageInfo);

    static String identifierForFile(File const &file);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_PACKAGE_H
