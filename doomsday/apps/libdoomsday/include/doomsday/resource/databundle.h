/** @file databundle.h  Classic data files: PK3, WAD, LMP, DED, DEH.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_DATABUNDLE_H
#define LIBDOOMSDAY_DATABUNDLE_H

#include "../libdoomsday.h"
#include "lumpdirectory.h"
#include <de/filesys/IInterpreter>
#include <de/Folder>

/**
 * Abstract base class for classic data files: PK3, WAD, LMP, DED, DEH.
 *
 * Specialized libcore Files representing loadable data files should be derived
 * from DataBundle. The class provides general functionality suitable for
 * dealing with all types of data files.
 *
 * Generates Doomsday 2 compatible metadata for data files, allowing them to
 * be treated as packages at runtime.
 *
 * When loaded as a package, DataBundle-based files make sure that the data
 * gets loaded and unloaded when games are loaded (and in the right order). In
 * practice, the data is loaded using the resource subsystem. To facilitate
 * that, DataBundle contents are available as plain byte arrays.
 *
 * DataBundle is derived from IObject; the object namespace maps to the file's
 * "package" subrecord.
 */
class LIBDOOMSDAY_PUBLIC DataBundle : public de::IByteArray
                                    , public de::IObject
{
public:
    enum Format { Unknown, Pk3, Wad, Iwad, Pwad, Lump, Ded, Dehacked, Collection };

    DENG2_ERROR(FormatError);
    DENG2_ERROR(LinkError);

    struct LIBDOOMSDAY_PUBLIC Interpreter : public de::filesys::IInterpreter {
        de::File *interpretFile(de::File *sourceData) const override;
    };

public:
    DataBundle(Format format, de::File &source);
    ~DataBundle();

    Format format() const;
    de::String description() const;
    de::File &asFile();
    de::File const &asFile() const;
    de::File const &sourceFile() const;

    /**
     * Identifier of the package representing this data bundle (after being identified).
     */
    de::String packageId() const;

    /**
     * Generates appropriate packages according to the contents of the data bundle.
     * @return @c true, if the bundle was identid and linked as a package.
     */
    bool identifyPackages() const;

    /**
     * Determines if the data bundle has been identified and now available as a package
     * link.
     */
    bool isLinkedAsPackage() const;

    /**
     * Returns the metadata record of the package representing this bundle. The bundle
     * must be linked as a package.
     */
    de::Record &packageMetadata();

    de::Record const &packageMetadata() const;

    /**
     * Determines if the bundle is nested inside another bundle.
     */
    bool isNested() const;

    /**
     * Finds the bundle that contains this bundle, if this bundle is a
     * nested one.
     *
     * @return DataBundle that contains this bundle, or @c nullptr if not
     * nested. Ownership not transferred.
     */
    DataBundle *containerBundle() const;

    /**
     * Finds the Package that contains this bunle, if this bundle is inside
     * a package.
     *
     * @return Package identifier, or an empty string.
     */
    de::String containerPackageId() const;

    /**
     * Returns the WAD file lump directory.
     * @return LumpDirectory for WADs; @c nullptr for non-WAD formats.
     */
    res::LumpDirectory const *lumpDirectory() const;

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, Byte const *values, Size count);

    // Implements IObject.
    virtual de::Record &objectNamespace();
    virtual de::Record const &objectNamespace() const;

    /**
     * Checks the data bundle format of a package, if the package represents a bundle.
     * The package can be currently loaded or unloaded.
     *
     * @param packageId  Identifier.
     * @return Bundle format.
     */
    static Format packageBundleFormat(de::String const &packageId);

    static DataBundle const *bundleForPackage(de::String const &packageId);

    /**
     * Compiles a list of all data bundles that have been loaded via
     * PackageLoader. The order of the list reflects the order in which
     * PackageLoader::load() was called on the packges.
     *
     * @return List of bundles.
     */
    static QList<DataBundle const *> loadedBundles();

protected:
    void setFormat(Format format);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDOOMSDAY_DATABUNDLE_H
