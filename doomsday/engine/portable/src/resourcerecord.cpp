/**
 * @file resourcerecord.cpp
 *
 * Resource Record.
 *
 * @ingroup resource
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
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
#include "zip.h"

#include "resourcerecord.h"

namespace de {

struct ResourceRecord::Instance
{
    /// Class of resource.
    resourceclass_t rclass;

    /// @see resourceFlags.
    int flags;

    /// Potential names for this resource. In precedence order - high (newest) to lowest.
    QStringList names;

    /// Vector of resource identifier keys (e.g., file or lump names).
    /// Used for identification purposes.
    QStringList identityKeys;

    /// Paths to use when attempting to locate this resource.
    Uri** searchPaths;

    /// Id+1 of the search path used to locate this resource (in _searchPaths) if found.
    /// Set during resource location.
    uint searchPathUsed;

    /// Fully resolved absolute path to the located resource if found.
    /// Set during resource location.
    QString foundPath;

    Instance(resourceclass_t _rclass, int rflags)
        : rclass(_rclass), flags(rflags & ~RF_FOUND), names(),
          identityKeys(), searchPaths(0), searchPathUsed(0), foundPath()
    {}

    ~Instance()
    {
        clearSearchPaths();
    }

    inline void clearSearchPaths()
    {
        F_DestroyUriList(reinterpret_cast<uri_s**>(searchPaths)); searchPaths = 0;
    }
};

ResourceRecord::ResourceRecord(resourceclass_t rclass, int rflags, QString* name)
{
    d = new Instance(rclass, rflags);
    if(name) addName(*name);
}

ResourceRecord::~ResourceRecord()
{
    delete d;
}

ResourceRecord& ResourceRecord::addName(QString newName, bool* didAdd)
{
    // Is this name unique? We don't want duplicates.
    if(newName.isEmpty() || d->names.contains(newName, Qt::CaseInsensitive))
    {
        if(didAdd) *didAdd = false;
        return *this;
    }

    // Add the new name.
    d->names.prepend(newName);

    // A new name means we may now be able to locate it - clear the cached paths.
    d->clearSearchPaths();

    if(didAdd) *didAdd = true;
    return *this;
}

ResourceRecord& ResourceRecord::addIdentityKey(QString newIdentityKey, bool* didAdd)
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

/// @return  @c true, iff the resource appears to be what we think it is.
static bool recognizeWAD(QString const& filePath, QStringList const& identityKeys)
{
    QByteArray filePathUtf8 = filePath.toUtf8();
    lumpnum_t auxLumpBase = App_FileSystem()->openAuxiliary(filePathUtf8.constData());
    bool result = false;

    if(auxLumpBase >= 0)
    {
        // Ensure all identity lumps are present.
        if(identityKeys.count())
        {
            result = true;
            DENG2_FOR_EACH_CONST(QStringList, i, identityKeys)
            {
                QByteArray identityKey = i->toUtf8();
                lumpnum_t lumpNum = App_FileSystem()->lumpNumForName(identityKey.constData());
                if(lumpNum < 0)
                {
                    result = false;
                    break;
                }
            }
        }
        else
        {
            // Matched.
            result = true;
        }

        F_CloseAuxiliary();
    }
    return result;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool recognizeZIP(QString const& filePath, QStringList const& /*identityKeys*/)
{
    try
    {
        QByteArray filePathUtf8 = filePath.toUtf8();
        FileHandle& hndl = App_FileSystem()->openFile(filePathUtf8.constData(), "rbf");
        bool result = Zip::recognise(hndl);
        /// @todo Check files. We should implement an auxiliary zip lump index...
        App_FileSystem()->releaseFile(hndl.file());
        delete &hndl;
        return result;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.
    return false;
}

ResourceRecord& ResourceRecord::locateResource()
{
    // Already found?
    if(d->flags & RF_FOUND) return *this;

    // Collate search paths.
    if(!d->searchPaths)
    {
        QByteArray nameString = d->names.join(";").toUtf8();
        d->searchPaths = reinterpret_cast<de::Uri**>(F_CreateUriList(d->rclass, nameString.constData()));
    }

    // Perform the search.
    int searchPathIdx = 0;
    for(Uri* const* ptr = d->searchPaths; *ptr; ptr++, searchPathIdx++)
    {
        de::Uri const* list[2] = { *ptr, NULL };

        // Attempt to resolve a path to the named resource.
        AutoStr* found = AutoStr_NewStd();
        if(!F_FindResource5(d->rclass, reinterpret_cast<uri_s const**>(list), found,
                            RLF_DEFAULT, NULL/*no optional suffix*/)) continue;

        // We've found *something*.
        String foundPath = String(Str_Text(found));

        // Perform identity validation.
        bool validated = false;
        if(d->rclass == RC_PACKAGE)
        {
            /// @todo The identity configuration should declare the type of resource...
                validated = recognizeWAD(foundPath, d->identityKeys);
            if(!validated)
                validated = recognizeZIP(foundPath, d->identityKeys);
        }
        else
        {
            // Other resource types are not validated.
            validated = true;
        }

        if(!validated) continue;

        // This is the resource we've been looking for.
        d->flags |= RF_FOUND;
        d->foundPath = foundPath;
        d->searchPathUsed = searchPathIdx + 1;
        break;
    }

    return *this;
}

ResourceRecord& ResourceRecord::forgetResource()
{
    if(d->flags & RF_FOUND)
    {
        d->foundPath.clear();
        d->searchPathUsed = 0;
        d->flags &= ~RF_FOUND;
    }
    return *this;
}

QString const& ResourceRecord::resolvedPath(bool tryLocate)
{
    if(tryLocate)
    {
        locateResource();
    }
    return d->foundPath;
}

resourceclass_t ResourceRecord::resourceClass() const
{
    return d->rclass;
}

int ResourceRecord::resourceFlags() const
{
    return d->flags;
}

QStringList const& ResourceRecord::identityKeys() const
{
    return d->identityKeys;
}

QStringList const& ResourceRecord::names() const
{
    return d->names;
}

void ResourceRecord::consolePrint(ResourceRecord& record, bool showStatus)
{
    QByteArray names = record.names().join(";").toUtf8();
    bool const resourceFound = !!(record.resourceFlags() & RF_FOUND);

    if(showStatus)
        Con_Printf("%s", !resourceFound? " ! ":"   ");

    Con_PrintPathList4(names.constData(), ';', " or ", PPF_TRANSFORM_PATH_MAKEPRETTY);

    if(showStatus)
    {
        QByteArray foundPath = resourceFound? record.resolvedPath(false/*don't try to locate*/).toUtf8() : QByteArray("");
        Con_Printf(" %s%s", !resourceFound? "- missing" : "- found ", F_PrettyPath(foundPath.constData()));
    }
    Con_Printf("\n");
}

} // namespace de
