/** @file package.h  Package containing metadata, data, and/or files.
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

#ifndef LIBCORE_PACKAGE_H
#define LIBCORE_PACKAGE_H

#include "de/string.h"
#include "de/file.h"
#include "de/fileindex.h"
#include "de/version.h"
#include "de/set.h"
#include "de/scripting/iobject.h"

namespace de {

class Package;

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
class DE_PUBLIC Package : public IObject
{
public:
    /// Package's source is missing or inaccessible. @ingroup errors
    DE_ERROR(SourceError);

    /// Package fails validation. @ingroup errors
    DE_ERROR(ValidationError);

    /// Checked metadata does not describe a package. @ingroup errors
    DE_SUB_ERROR(ValidationError, NotPackageError);

    /// Package is missing some required metadata. @ingroup errors
    DE_SUB_ERROR(ValidationError, IncompleteMetadataError);

    typedef Set<String> Assets;

    /**
     * Utility for accessing asset metadata. @ingroup fs
     */
    class DE_PUBLIC Asset : public RecordAccessor
    {
    public:
        Asset(const Record &rec);
        Asset(const Record *rec);

        /**
         * Retrieves the value of a variable and resolves it to an absolute path in
         * relation to the asset.
         *
         * @param varName  Variable name in the package asset metadata.
         *
         * @return Absolute path.
         *
         * @see ScriptedInfo::absolutePathInContext()
         */
        String absolutePath(const String &varName) const;
    };

public:
    /**
     * Creates a package whose data comes from a file. The file's metadata is used
     * as the package's metadata namespace.
     *
     * @param file  File or folder containing the package's contents.
     */
    Package(const File &file);

    virtual ~Package();

    /**
     * Returns the ".pack" file of the Package. In practice, this may be a ZIP folder, an
     * regular folder, or a link to a DataBundle. You should use sourceFile() to access
     * the file in which the package's contents are actually stored.
     *
     * @return Package file.
     */
    const File &file() const;

    /**
     * Returns the original source file of the package, where the package's contents are
     * being sourced from. This is usually the file referenced by the "path" member in
     * the package metadata.
     */
    const File &sourceFile() const;

    bool sourceFileExists() const;

    /**
     * Returns the package's root folder.
     */
    const Folder &root() const;

    /**
     * Returns the unique package identifier. This is the file name of the package
     * without any file extension.
     */
    String identifier() const;

    /**
     * Version of the loaded package. The version can be specified either in the
     * file name (following an underscore) or in the metadata.
     */
    Version version() const;

    /**
     * Composes a list of assets contained in the package.
     *
     * @return Assets declared in the package metadata.
     */
    Assets assets() const;

    /**
     * Executes a script function in the metadata of the package.
     *
     * @param name  Name of the function to call.
     *
     * @return @c true, if the function exists and was called. @c false, if the
     * function was not found.
     */
    bool executeFunction(const String &name);

    void setOrder(int ordinal);

    int order() const;

    void findPartialPath(const String &path, FileIndex::FoundFiles &found) const;

    /**
     * Called by PackageLoader after the package has been marked as loaded.
     */
    virtual void didLoad();

    /**
     * Called by PackageLoader immediately before the package is marked as unloaded.
     */
    virtual void aboutToUnload();

    // Implements IObject.
    Record &objectNamespace();
    const Record &objectNamespace() const;

public:
    /**
     * Parse the embedded metadata found in a package file.
     *
     * @param packageFile  File containing a package.
     */
    static void parseMetadata(File &packageFile);

    /**
     * Checks that all the metadata seems legit. An IncompleteMetadataError or
     * another exception is thrown if the package is not deemed valid.
     *
     * @param packageInfo  Metadata to validate.
     */
    static void validateMetadata(const Record &packageInfo);

    static Record &initializeMetadata(File &packageFile, const String &id = String());

    static const Record &metadata(const File &packageFile);

    static StringList tags(const File &packageFile);

    static bool matchTags(const File &packageFile, const String &tagRegExp);

    static StringList tags(String const& tagsString);

    static StringList requiredPackages(const File &packageFile);

    static void addRequiredPackage(File &packageFile, const String &id);

    static bool hasOptionalContent(const String &packageId);

    static bool hasOptionalContent(const File &packageFile);

    /**
     * Splits a string containing a package identifier and version. The
     * expected format of the string is `{packageId}_{version}`.
     *
     * @param identifier_version  Identifier and version.
     *
     * @return The split components.
     */
    static std::pair<String, Version> split(const String &identifier_version);

    static String splitToHumanReadable(const String &identifier_version);

    static bool equals(const String &id1, const String &id2);

    static String identifierForFile(const File &file);

    static String versionedIdentifierForFile(const File &file);

    static Version versionForFile(const File &file);

    /**
     * Locates the file that represents the package where @a file is in.
     *
     * @param file  File.
     *
     * @return Containing package, or nullptr if the file is not inside a package.
     */
    static const File *containerOfFile(const File &file);

    static String identifierForContainerOfFile(const File &file);

    /**
     * Finds the package that contains @a file and returns its modification time.
     * If the file doesn't appear to be inside a package, returns the file's
     * modification time.
     *
     * @param file  File.
     *
     * @return Modification time of file or package.
     */
    static Time containerOfFileModifiedAt(const File &file);

    static const String VAR_PACKAGE;
    static const String VAR_PACKAGE_ID;
    static const String VAR_PACKAGE_ALIAS;
    static const String VAR_PACKAGE_TITLE;
    static const String VAR_ID;
    static const String VAR_TITLE;
    static const String VAR_VERSION;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_PACKAGE_H
