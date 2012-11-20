/**
 * @file sys_reslocator.cpp
 *
 * Resource location algorithms and bookeeping.
 *
 * @ingroup resources
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <QDir>
#include <QList>
#include <QStringList>

#include "de_base.h"
#include "de_filesys.h"
#include "de_resource.h"

#include <de/Error>
#include <de/App>
#include <de/CommandLine>
#include <de/NativePath>
#include <de/memory.h>

using namespace de;

static bool tryFindInNamespace(de::Uri const& searchPath, FS1::Namespace& fnamespace,
    ddstring_t* foundPath)
{
    try
    {
        PathTree::Node& node = App_FileSystem()->find(searchPath, fnamespace);
        // Does the caller want to know the matched path?
        if(foundPath)
        {
            QByteArray path = node.composePath().toUtf8();
            Str_Set(foundPath, path.constData());
            F_PrependBasePath(foundPath, foundPath);
        }
        return true;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    return false;
}

static bool tryFind(de::Uri const& searchPath, ddstring_t* foundPath)
{
    try
    {
        String searchPathStr = searchPath.compose();
        if(App_FileSystem()->accessFile(searchPathStr))
        {
            // Does the caller want to know the matched path?
            if(foundPath)
            {
                QByteArray searchPathUtf8 = searchPathStr.toUtf8();
                Str_Set(foundPath, searchPathUtf8.constData());
                F_PrependBasePath(foundPath, foundPath);
            }
            return true;
        }
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    return false;
}

static bool findFile3(FS1::Namespace* fnamespace, de::Uri const& searchPath,
    ddstring_t* foundPath)
{
    // Is there a namespace we should use?
    if(fnamespace)
    {
        if(tryFindInNamespace(searchPath, *fnamespace, foundPath))
            return true;
    }

    return tryFind(searchPath, foundPath);
}

static bool findFile2(int flags, resourceclassid_t classId, String searchPath,
    ddstring_t* foundPath, FS1::Namespace* fnamespace)
{
    if(searchPath.isEmpty()) return false;

    // If an extension was specified, first look for files of the same type.
    String ext = searchPath.fileNameExtension();
    if(!ext.isEmpty() && ext.compare(".*"))
    {
        if(findFile3(fnamespace, de::Uri(searchPath, RC_NULL), foundPath)) return true;

        // If we are looking for a particular file type, get out of here.
        if(flags & RLF_MATCH_EXTENSION) return false;
    }

    ResourceClass const& rclass = DD_ResourceClassById(classId);
    if(!rclass.fileTypeCount()) return false;

    /*
     * Try some different name patterns (i.e., file types) known to us.
     */

    // Create a copy of the searchPath minus file extension.
    String path2 = searchPath.fileNamePath() / searchPath.fileNameWithoutExtension();

    path2.reserve(path2.length() + 5 /*max expected extension length*/);

    DENG2_FOR_EACH_CONST(ResourceClass::Types, typeIt, rclass.fileTypes())
    {
        DENG2_FOR_EACH_CONST(QStringList, i, (*typeIt)->knownFileNameExtensions())
        {
            String const& ext = *i;
            if(findFile3(fnamespace, de::Uri(path2 + ext, RC_NULL), foundPath))
            {
                return true;
            }
        }
    };

    return false; // Not found.
}

static bool findFile(resourceclassid_t classId, de::Uri const& searchPath,
    ddstring_t* foundPath, int flags, String optionalSuffix = "")
{
    DENG_ASSERT(classId == RC_UNKNOWN || VALID_RESOURCECLASSID(classId));

    LOG_AS("findFile");

    if(searchPath.isEmpty()) return false;

    try
    {
        String const& resolvedPath = searchPath.resolved();

        // Is a namespace specified?
        FS1::Namespace* fnamespace = App_FileSystem()->namespaceByName(searchPath.scheme());

        // First try with the optional suffix.
        if(!optionalSuffix.isEmpty())
        {
            String resolvedPath2 = resolvedPath.fileNamePath()
                                 / resolvedPath.fileNameWithoutExtension() + optionalSuffix + resolvedPath.fileNameExtension();

            if(findFile2(flags, classId, resolvedPath2, foundPath, fnamespace))
                return true;
        }

        // Try without a suffix.
        return findFile2(flags, classId, resolvedPath, foundPath, fnamespace);
    }
    catch(de::Uri::ResolveError const& er)
    {
        // Log but otherwise ignore incomplete paths.
        LOG_DEBUG(er.asText());
    }
    return false;
}

boolean F_Find4(resourceclassid_t classId, uri_s const* searchPath,
    ddstring_t* foundPath, int flags, char const* optionalSuffix)
{
    DENG_ASSERT(searchPath);
    return findFile(classId, reinterpret_cast<de::Uri const&>(*searchPath), foundPath, flags, optionalSuffix);
}

boolean F_Find3(resourceclassid_t classId, uri_s const* searchPath,
    ddstring_t* foundPath, int flags)
{
    return F_Find4(classId, searchPath, foundPath, flags, NULL);
}

boolean F_Find2(resourceclassid_t classId, uri_s const* searchPath,
    ddstring_t* foundPath)
{
    return F_Find3(classId, searchPath, foundPath, RLF_DEFAULT);
}

boolean F_Find(resourceclassid_t classId, uri_s const* searchPath)
{
    return F_Find2(classId, searchPath, NULL);
}

uint F_FindFromList(resourceclassid_t classId, char const* searchPaths,
    ddstring_t* foundPath, int flags, char const* optionalSuffix)
{
    if(!searchPaths || !searchPaths[0]) return 0;

    QStringList paths = String(searchPaths).split(';', QString::SkipEmptyParts);
    int pathIndex = 0;
    for(QStringList::const_iterator i = paths.constBegin(); i != paths.constEnd(); ++i, ++pathIndex)
    {
        de::Uri searchPath = de::Uri(*i, classId);
        if(findFile(classId, searchPath, foundPath, flags, optionalSuffix))
        {
            return pathIndex + 1; // 1-based index.
        }
    }

    return 0; // Not found.
}
