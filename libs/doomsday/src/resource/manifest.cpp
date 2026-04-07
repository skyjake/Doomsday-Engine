/** @file manifest.cpp  Game resource manifest.
 *
 * @authors Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/manifest.h"
#include "doomsday/res/resources.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/lumpindex.h"
#include "doomsday/filesys/wad.h"
#include "doomsday/filesys/zip.h"

#include <de/app.h>
#include <de/filesystem.h>
#include <de/nativefile.h>
#include <de/path.h>

using namespace de;
using namespace res;

DE_PIMPL(ResourceManifest)
{
    resourceclassid_t classId;

    int flags;        ///< @ref fileFlags.
    StringList names; ///< Known names in precedence order.

    /// Vector of resource identifier keys (e.g., file or lump names).
    /// Used for identification purposes.
    StringList identityKeys;

    /// Index (in Manifest::Impl::names) of the name used to locate
    /// this resource if found. Set during resource location.
    int foundNameIndex;

    /// Fully resolved absolute path to the located resource if found.
    /// Set during resource location.
    String foundPath;

    Impl(Public *i, resourceclassid_t rclass, int rflags)
        : Base(i)
        , classId(rclass)
        , flags(rflags & ~FF_FOUND)
        , names()
        , identityKeys()
        , foundNameIndex(-1)
        , foundPath()
    {}
};

ResourceManifest::ResourceManifest(resourceclassid_t resClass, int fFlags, String *name)
    : d(new Impl(this, resClass, fFlags))
{
    if (name) addName(*name);
}

void ResourceManifest::addName(const String& newName)
{
    if (newName.isEmpty()) return;

    // Is this name unique? We don't want duplicates.
    if (!String::contains(d->names, newName))
    {
        d->names.prepend(newName);
    }
}

void ResourceManifest::addIdentityKey(const String& newIdKey)
{
    if (newIdKey.isEmpty()) return;

    // Is this key unique? We don't want duplicates.
    if (!String::contains(d->identityKeys, newIdKey))
    {
        d->identityKeys.append(newIdKey);
    }
}

enum lumpsizecondition_t
{
    LSCOND_NONE,
    LSCOND_EQUAL,
    LSCOND_GREATER_OR_EQUAL,
    LSCOND_LESS_OR_EQUAL
};

/**
 * Modifies the idKey so that the size condition is removed.
 */
static void checkSizeConditionInIdentityKey(String &idKey, lumpsizecondition_t *pCond, size_t *pSize)
{
    DE_ASSERT(pCond != 0);
    DE_ASSERT(pSize != 0);

    *pCond = LSCOND_NONE;
    *pSize = 0;

    BytePos condPos;
    BytePos argPos;
    if (auto condPos = idKey.indexOf("=="))
    {
        *pCond = LSCOND_EQUAL;
        argPos = condPos + 2;
    }
    else if (auto condPos = idKey.indexOf(">="))
    {
        *pCond = LSCOND_GREATER_OR_EQUAL;
        argPos = condPos + 2;
    }
    else if (auto condPos = idKey.indexOf("<="))
    {
        *pCond = LSCOND_LESS_OR_EQUAL;
        argPos = condPos + 2;
    }

    if (!condPos) return;

    // Get the argument.
    *pSize = strtoul(idKey.substr(argPos), nullptr, 0);

    // Remove it from the name.
    idKey.truncate(condPos);
}

static lumpnum_t lumpNumForIdentityKey(const LumpIndex &lumpIndex, String idKey)
{
    if (idKey.isEmpty()) return -1;

    // The key may contain a size condition (==, >=, <=).
    lumpsizecondition_t sizeCond;
    size_t refSize;
    checkSizeConditionInIdentityKey(idKey, &sizeCond, &refSize);

    // We should now be left with just the name.
    String name = idKey;

    // Append a .lmp extension if none is specified.
    if (idKey.fileNameExtension().isEmpty())
    {
        name += ".lmp";
    }

    lumpnum_t lumpNum = lumpIndex.findLast(Path(name));
    if (lumpNum < 0) return -1;

    // Check the condition.
    size_t lumpSize = lumpIndex[lumpNum].info().size;
    switch (sizeCond)
    {
    case LSCOND_EQUAL:
        if (lumpSize != refSize) return -1;
        break;

    case LSCOND_GREATER_OR_EQUAL:
        if (lumpSize < refSize) return -1;
        break;

    case LSCOND_LESS_OR_EQUAL:
        if (lumpSize > refSize) return -1;
        break;

    default: break;
    }

    return lumpNum;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool validateWad(const String &filePath, const StringList &identityKeys)
{
    bool validated = true;
    try
    {
        FileHandle &hndl = App_FileSystem().openFile(filePath, "rb", 0/*baseOffset*/, true /*allow duplicates*/);

        if (Wad *wad = maybeAs<Wad>(hndl.file()))
        {
            // Ensure all identity lumps are present.
            if (identityKeys.count())
            {
                if (wad->isEmpty())
                {
                    // Clear not what we are looking for.
                    validated = false;
                }
                else
                {
                    // Publish lumps to a temporary index.
                    LumpIndex lumpIndex;
                    for (int i = 0; i < wad->lumpCount(); ++i)
                    {
                        lumpIndex.catalogLump(wad->lump(i));
                    }

                    // Check each lump.
                    for (const auto &i : identityKeys)
                    {
                        if (lumpNumForIdentityKey(lumpIndex, i) < 0)
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
    catch (const FS1::NotFoundError &)
    {} // Ignore this error.

    return validated;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool validateZip(const String &filePath, const StringList & /*identityKeys*/)
{
    try
    {
        FileHandle &hndl = App_FileSystem().openFile(filePath, "rbf");
        bool result = Zip::recognise(hndl);
        /// @todo Check files. We should implement an auxiliary zip lump index...
        App_FileSystem().releaseFile(hndl.file());
        delete &hndl;
        return result;
    }
    catch (const FS1::NotFoundError &)
    {} // Ignore error.
    return false;
}

void ResourceManifest::locateFile()
{
    // Already found?
    if (d->flags & FF_FOUND) return;

    // Perform the search.
    int nameIndex = 0;
    for (auto i = d->names.begin(); i != d->names.end(); ++i, ++nameIndex)
    {
        StringList candidates;

        // Attempt to resolve a path to the named resource using FS1.
        try
        {
            String foundPath = App_FileSystem().findPath(res::Uri(*i, d->classId),
                                                         RLF_DEFAULT, App_ResourceClass(d->classId));
            foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.
            candidates << foundPath;
        }
        catch (const FS1::NotFoundError &)
        {} // Ignore this error.

        // Also check what FS2 has to offer. FS1 can't access FS2's files, so we'll
        // restrict this to native files.
        App::fileSystem().forAll(*i, [&candidates] (File &f)
        {
            // We ignore interpretations and go straight to the source.
            if (const NativeFile *native = maybeAs<NativeFile>(f.source()))
            {
                candidates << native->nativePath();
            }
            return LoopContinue;
        });

        for (const String &foundPath : candidates)
        {
            // Perform identity validation.
            bool validated = false;
            if (d->classId == RC_PACKAGE)
            {
                /// @todo The identity configuration should declare the type of resource...
                validated = validateWad(foundPath, d->identityKeys);
                if (!validated)
                    validated = validateZip(foundPath, d->identityKeys);
            }
            else
            {
                // Other resource types are not validated.
                validated = true;
            }

            if (validated)
            {
                // This is the resource we've been looking for.
                d->flags |= FF_FOUND;
                d->foundPath = foundPath;
                d->foundNameIndex = nameIndex;
                return;
            }
        }
    }
}

void ResourceManifest::forgetFile()
{
    if (d->flags & FF_FOUND)
    {
        d->foundPath.clear();
        d->foundNameIndex = -1;
        d->flags &= ~FF_FOUND;
    }
}

const String &ResourceManifest::resolvedPath(bool tryLocate)
{
    if (tryLocate)
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

const StringList &ResourceManifest::identityKeys() const
{
    return d->identityKeys;
}

const StringList &ResourceManifest::names() const
{
    return d->names;
}
