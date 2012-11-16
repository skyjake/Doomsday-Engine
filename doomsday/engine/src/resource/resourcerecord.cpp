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
#include "filesys/zip.h"

#include "resourcerecord.h"

namespace de {

struct ResourceRecord::Instance
{
    /// Class of resource.
    resourceclassid_t classId;

    /// @see resourceFlags.
    int flags;

    /// Potential names for this resource. In precedence order - high (newest) to lowest.
    QStringList names;

    /// Vector of resource identifier keys (e.g., file or lump names).
    /// Used for identification purposes.
    QStringList identityKeys;

    /// Index (in ResourceRecord::Instance::names) of the name used to locate
    /// this resource if found. Set during resource location.
    int foundNameIndex;

    /// Fully resolved absolute path to the located resource if found.
    /// Set during resource location.
    String foundPath;

    Instance(resourceclassid_t _rclass, int rflags)
        : classId(_rclass), flags(rflags & ~RF_FOUND), names(),
          identityKeys(), foundNameIndex(-1), foundPath()
    {}
};

ResourceRecord::ResourceRecord(resourceclassid_t classId, int rflags, String* name)
{
    d = new Instance(classId, rflags);
    if(name) addName(*name);
}

ResourceRecord::~ResourceRecord()
{
    delete d;
}

ResourceRecord& ResourceRecord::addName(String newName, bool* didAdd)
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

ResourceRecord& ResourceRecord::addIdentityKey(String newIdentityKey, bool* didAdd)
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
static bool recognizeWAD(String const& filePath, QStringList const& identityKeys)
{
    lumpnum_t auxLumpBase = App_FileSystem()->openAuxiliary(filePath);
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
static bool recognizeZIP(String const& filePath, QStringList const& /*identityKeys*/)
{
    try
    {
        FileHandle& hndl = App_FileSystem()->openFile(filePath, "rbf");
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

    // Perform the search.
    AutoStr* found = AutoStr_NewStd();
    int nameIndex = 0;
    for(QStringList::const_iterator i = d->names.constBegin(); i != d->names.constEnd(); ++i, ++nameIndex)
    {
        Uri path = Uri(*i, d->classId);

        // Attempt to resolve a path to the named resource.
        if(!F_FindResource2(d->classId, reinterpret_cast<uri_s*>(&path), found)) continue;

        // We've found *something*.
        String foundPath = String(Str_Text(found));

        // Perform identity validation.
        bool validated = false;
        if(d->classId == RC_PACKAGE)
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
        d->foundNameIndex = nameIndex;
        break;
    }

    return *this;
}

ResourceRecord& ResourceRecord::forgetResource()
{
    if(d->flags & RF_FOUND)
    {
        d->foundPath.clear();
        d->foundNameIndex = -1;
        d->flags &= ~RF_FOUND;
    }
    return *this;
}

String const& ResourceRecord::resolvedPath(bool tryLocate)
{
    if(tryLocate)
    {
        locateResource();
    }
    return d->foundPath;
}

resourceclassid_t ResourceRecord::resourceClass() const
{
    return d->classId;
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
