/** @file manifest.cpp Game Resource Manifest.
 *
 * @authors Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "resource/wad.h"
#include "resource/zip.h"
#include <de/Path>

#include "filesys/manifest.h"

using namespace de;

DENG2_PIMPL(ResourceManifest)
{
    /// Class of resource.
    resourceclassid_t classId;

    /// @see fileFlags.
    int flags;

    /// Potential names for this resource. In precedence order - high (newest) to lowest.
    QStringList names;

    /// Vector of resource identifier keys (e.g., file or lump names).
    /// Used for identification purposes.
    QStringList identityKeys;

    /// Index (in Manifest::Instance::names) of the name used to locate
    /// this resource if found. Set during resource location.
    int foundNameIndex;

    /// Fully resolved absolute path to the located resource if found.
    /// Set during resource location.
    String foundPath;

    Instance(Public *i, resourceclassid_t _rclass, int rflags) : Base(i),
        classId(_rclass),
        flags(rflags & ~FF_FOUND),
        names(),
        identityKeys(),
        foundNameIndex(-1),
        foundPath()
    {}
};

ResourceManifest::ResourceManifest(resourceclassid_t fClass, int fFlags, String *name)
    : d(new Instance(this, fClass, fFlags))
{
    if(name) addName(*name);
}

ResourceManifest::~ResourceManifest()
{
    delete d;
}

ResourceManifest &ResourceManifest::addName(String newName, bool *didAdd)
{
    // Is this name unique? We don't want duplicates.
    if(newName.isEmpty() || d->names.contains(newName, Qt::CaseInsensitive))
    {
        if(didAdd) *didAdd = false;
        return *this;
    }

    // Add the new name.
    d->names.prepend(newName);

    if(didAdd) *didAdd = true;
    return *this;
}

ResourceManifest &ResourceManifest::addIdentityKey(String newIdentityKey, bool *didAdd)
{
    // Is this key unique? We don't want duplicates.
    if(newIdentityKey.isEmpty() || d->identityKeys.contains(newIdentityKey, Qt::CaseInsensitive))
    {
        if(didAdd) *didAdd = false;
        return *this;
    }

    // Add the new key.
    d->identityKeys.append(newIdentityKey);

    if(didAdd) *didAdd = true;
    return *this;
}

typedef enum lumpsizecondition_e {
    LSCOND_NONE,
    LSCOND_EQUAL,
    LSCOND_GREATER_OR_EQUAL,
    LSCOND_LESS_OR_EQUAL
} lumpsizecondition_t;

/**
 * Modifies the idKey so that the size condition is removed.
 */
static void checkSizeConditionInIdentityKey(String& idKey, lumpsizecondition_t* pCond, size_t* pSize)
{
    DENG_ASSERT(pCond != 0);
    DENG_ASSERT(pSize != 0);

    *pCond = LSCOND_NONE;
    *pSize = 0;

    int condPos = -1;
    int argPos  = -1;
    if((condPos = idKey.indexOf("==")) >= 0)
    {
        *pCond = LSCOND_EQUAL;
        argPos = condPos + 2;
    }
    else if((condPos = idKey.indexOf(">=")) >= 0)
    {
        *pCond = LSCOND_GREATER_OR_EQUAL;
        argPos = condPos + 2;
    }
    else if((condPos = idKey.indexOf("<=")) >= 0)
    {
        *pCond = LSCOND_LESS_OR_EQUAL;
        argPos = condPos + 2;
    }

    if(condPos < 0) return;

    // Get the argument.
    *pSize = idKey.mid(argPos).toULong();

    // Remove it from the name.
    idKey.truncate(condPos);
}

static lumpnum_t lumpNumForIdentityKey(LumpIndex const& lumpIndex, String idKey)
{
    if(idKey.isEmpty()) return -1;

    // The key may contain a size condition (==, >=, <=).
    lumpsizecondition_t sizeCond;
    size_t refSize;
    checkSizeConditionInIdentityKey(idKey, &sizeCond, &refSize);

    // We should now be left with just the name.
    String name = idKey;

    // Append a .lmp extension if none is specified.
    if(idKey.fileNameExtension().isEmpty())
    {
        name += ".lmp";
    }

    lumpnum_t lumpNum = lumpIndex.lastIndexForPath(Path(name));
    if(lumpNum < 0) return -1;

    // Check the condition.
    size_t lumpSize = lumpIndex.lump(lumpNum).info().size;
    switch(sizeCond)
    {
    case LSCOND_EQUAL:
        if(lumpSize != refSize) return -1;
        break;

    case LSCOND_GREATER_OR_EQUAL:
        if(lumpSize < refSize) return -1;
        break;

    case LSCOND_LESS_OR_EQUAL:
        if(lumpSize > refSize) return -1;
        break;

    default: break;
    }

    return lumpNum;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool validateWad(String const &filePath, QStringList const &identityKeys)
{
    bool validated = true;
    try
    {
        de::FileHandle &hndl = App_FileSystem().openFile(filePath, "rb", 0/*baseOffset*/, true /*allow duplicates*/);

        if(Wad *wad = dynamic_cast<Wad *>(&hndl.file()))
        {
            // Ensure all identity lumps are present.
            if(identityKeys.count())
            {
                if(wad->empty())
                {
                    // Clear not what we are looking for.
                    validated = false;
                }
                else
                {
                    // Publish lumps to a temporary index.
                    LumpIndex lumpIndex;
                    for(int i = 0; i < wad->lumpCount(); ++i)
                    {
                        lumpIndex.catalogLump(wad->lump(i));
                    }

                    // Check each lump.
                    DENG2_FOR_EACH_CONST(QStringList, i, identityKeys)
                    {
                        if(lumpNumForIdentityKey(lumpIndex, *i) < 0)
                        {
                            validated = false;
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            validated = false;
        }

        // We're done with the file.
        App_FileSystem().releaseFile(hndl.file());
        delete &hndl;
    }
    catch(FS1::NotFoundError const &)
    {} // Ignore this error.

    return validated;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool validateZip(String const &filePath, QStringList const & /*identityKeys*/)
{
    try
    {
        de::FileHandle &hndl = App_FileSystem().openFile(filePath, "rbf");
        bool result = Zip::recognise(hndl);
        /// @todo Check files. We should implement an auxiliary zip lump index...
        App_FileSystem().releaseFile(hndl.file());
        delete &hndl;
        return result;
    }
    catch(FS1::NotFoundError const &)
    {} // Ignore error.
    return false;
}

ResourceManifest &ResourceManifest::locateFile()
{
    // Already found?
    if(d->flags & FF_FOUND) return *this;

    // Perform the search.
    int nameIndex = 0;
    for(QStringList::const_iterator i = d->names.constBegin(); i != d->names.constEnd(); ++i, ++nameIndex)
    {
        // Attempt to resolve a path to the named resource.
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri(*i, d->classId),
                                                          RLF_DEFAULT, DD_ResourceClassById(d->classId));
            foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.

            // Perform identity validation.
            bool validated = false;
            if(d->classId == RC_PACKAGE)
            {
                /// @todo The identity configuration should declare the type of resource...
                    validated = validateWad(foundPath, d->identityKeys);
                if(!validated)
                    validated = validateZip(foundPath, d->identityKeys);
            }
            else
            {
                // Other resource types are not validated.
                validated = true;
            }

            if(validated)
            {
                // This is the resource we've been looking for.
                d->flags |= FF_FOUND;
                d->foundPath = foundPath;
                d->foundNameIndex = nameIndex;
                break;
            }
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    return *this;
}

ResourceManifest &ResourceManifest::forgetFile()
{
    if(d->flags & FF_FOUND)
    {
        d->foundPath.clear();
        d->foundNameIndex = -1;
        d->flags &= ~FF_FOUND;
    }
    return *this;
}

String const &ResourceManifest::resolvedPath(bool tryLocate)
{
    if(tryLocate)
    {
        locateFile();
    }
    return d->foundPath;
}

resourceclassid_t ResourceManifest::resourceClass() const
{
    return d->classId;
}

int ResourceManifest::fileFlags() const
{
    return d->flags;
}

QStringList const &ResourceManifest::identityKeys() const
{
    return d->identityKeys;
}

QStringList const &ResourceManifest::names() const
{
    return d->names;
}

void ResourceManifest::consolePrint(ResourceManifest &manifest, bool showStatus)
{
    QByteArray names = manifest.names().join(";").toUtf8();
    bool const resourceFound = !!(manifest.fileFlags() & FF_FOUND);

    if(showStatus)
        Con_Printf("%s", !resourceFound? " ! ":"   ");

    Con_PrintPathList4(names.constData(), ';', " or ", PPF_TRANSFORM_PATH_MAKEPRETTY);

    if(showStatus)
    {
        QByteArray foundPath = resourceFound? manifest.resolvedPath(false/*don't try to locate*/).toUtf8() : QByteArray("");
        Con_Printf(" %s%s", !resourceFound? "- missing" : "- found ", F_PrettyPath(foundPath.constData()));
    }
    Con_Printf("\n");
}
