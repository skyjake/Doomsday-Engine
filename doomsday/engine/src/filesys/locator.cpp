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

class ZipFileType : public de::NativeFileType
{
public:
    ZipFileType() : NativeFileType("FT_ZIP", RC_PACKAGE)
    {}

    de::File1* interpret(de::FileHandle& hndl, String path, FileInfo const& info) const
    {
        if(Zip::recognise(hndl))
        {
            LOG_AS("ZipFileType");
            LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
            return new Zip(hndl, path, info);
        }
        return 0;
    }
};

class WadFileType : public de::NativeFileType
{
public:
    WadFileType() : NativeFileType("FT_WAD", RC_PACKAGE)
    {}

    de::File1* interpret(de::FileHandle& hndl, String path, FileInfo const& info) const
    {
        if(Wad::recognise(hndl))
        {
            LOG_AS("WadFileType");
            LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
            return new Wad(hndl, path, info);
        }
        return 0;
    }
};

static bool inited = false;

static NullResourceClass nullClass;
static NullFileType nullType;

static FileTypes types;
static ResourceClasses classes;
static FileNamespaces namespaces;

static inline ResourceClass& resourceClass(resourceclassid_t id)
{
    if(id == RC_NULL) return nullClass;
    if(!VALID_RESOURCECLASSID(id)) throw Error("resourceClass", String("Invalid id %1").arg(id));
    return *classes[uint(id)];
}

/**
 * @param name      Unique symbolic name of this namespace. Must be at least
 *                  @c FILENAMESPACE_MINNAMELENGTH characters long.
 * @param flags     @ref FileNamespace::Flag
 */
static FileNamespace& createFileNamespace(String name, FileNamespace::Flags flags = 0)
{
    DENG_ASSERT(name.length() >= FILENAMESPACE_MINNAMELENGTH);
    namespaces.push_back(new FileNamespace(name, flags));
    return *namespaces.back();
}

static bool findFileInNamespace(FileNamespace& fnamespace, de::Uri const& searchPath,
    ddstring_t* foundPath)
{
    if(searchPath.isEmpty()) return 0;

    LOG_TRACE("Using namespace '%s'...") << fnamespace.name();

    // Ensure the namespace is up to date.
    fnamespace.rebuild();

    // The in-namespace name is the file name sans extension.
    String name = searchPath.firstSegment().toString().fileNameWithoutExtension();

    // Perform the search.
    FileNamespace::FileList foundFiles;
    if(fnamespace.findAll(name, foundFiles))
    {
        // There is at least one name-matched (perhaps partially) file.
        DENG2_FOR_EACH_CONST(FileNamespace::FileList, i, foundFiles)
        {
            PathTree::Node& node = **i;
            if(!node.comparePath(searchPath, PCF_NO_BRANCH))
            {
                // This is the file we are looking for.
                // Does the caller want to know the matched path?
                if(foundPath)
                {
                    QByteArray path = node.composePath().toUtf8();
                    Str_Set(foundPath, path.constData());
                    F_PrependBasePath(foundPath, foundPath);
                }
                return true;
            }
        }
    }
    return false; // Not found.
}

static bool findFile4(de::Uri const& searchPath, ddstring_t* foundPath)
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

static bool findFile3(FileNamespace* fnamespace, de::Uri const& searchPath,
    ddstring_t* foundPath)
{
    // Is there a namespace we should use?
    if(fnamespace)
    {
        if(findFileInNamespace(*fnamespace, searchPath, foundPath))
            return true;
    }

    return findFile4(searchPath, foundPath);
}

static bool findFile2(int flags, resourceclassid_t classId, String searchPath,
    ddstring_t* foundPath, FileNamespace* fnamespace)
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

    ResourceClass const& rclass = resourceClass(classId);
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
        FileNamespace* fnamespace = F_FileNamespaceByName(searchPath.scheme());

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

static void createPackagesNamespace()
{
    FileNamespace& fnamespace = createFileNamespace("Packages");

    /*
     * Add default search paths.
     *
     * Note that the order here defines the order in which these paths are searched
     * thus paths must be added in priority order (newer paths have priority).
     */

#ifdef UNIX
    // There may be an iwadir specified in a system-level config file.
    filename_t fn;
    if(UnixInfo_GetConfigValue("paths", "iwaddir", fn, FILENAME_T_MAXLEN))
    {
        NativePath path = de::App::app().commandLine().startupPath() / fn;
        fnamespace.addSearchPath(FileNamespace::DefaultPaths, de::Uri::fromNativeDirPath(path), SPF_NO_DESCEND);
        LOG_INFO("Using paths.iwaddir: %s") << path.pretty();
    }
#endif

    // Add the path from the DOOMWADDIR environment variable.
    if(!CommandLine_Check("-nodoomwaddir") && getenv("DOOMWADDIR"))
    {
        NativePath path = de::App::app().commandLine().startupPath() / getenv("DOOMWADDIR");
        fnamespace.addSearchPath(FileNamespace::DefaultPaths, de::Uri::fromNativeDirPath(path), SPF_NO_DESCEND);
        LOG_INFO("Using DOOMWADDIR: %s") << path.pretty();
    }

    // Add any paths from the DOOMWADPATH environment variable.
    if(!CommandLine_Check("-nodoomwadpath") && getenv("DOOMWADPATH"))
    {
#if WIN32
#  define SEP_CHAR      ';'
#else
#  define SEP_CHAR      ':'
#endif

        QStringList allPaths = QString(getenv("DOOMWADPATH")).split(SEP_CHAR, QString::SkipEmptyParts);
        for(int i = allPaths.count(); i--> 0; )
        {
            NativePath path = de::App::app().commandLine().startupPath() / allPaths[i];
            fnamespace.addSearchPath(FileNamespace::DefaultPaths, de::Uri::fromNativeDirPath(path), SPF_NO_DESCEND);
            LOG_INFO("Using DOOMWADPATH: %s") << path.pretty();
        }

#undef SEP_CHAR
    }

    fnamespace.addSearchPath(FileNamespace::DefaultPaths, de::Uri("$(App.DataPath)/", RC_NULL), SPF_NO_DESCEND);
    fnamespace.addSearchPath(FileNamespace::DefaultPaths, de::Uri("$(App.DataPath)/$(GamePlugin.Name)/", RC_NULL), SPF_NO_DESCEND);
}

static void createFileNamespaces()
{
    int const namespacedef_max_searchpaths = 5;
    struct namespacedef_s {
        char const* name;
        char const* optOverridePath;
        char const* optFallbackPath;
        FileNamespace::Flags flags;
        int searchPathFlags; /// @see searchPathFlags
        /// Priority is right to left.
        char const* searchPaths[namespacedef_max_searchpaths];
    } defs[] = {
        { "Defs",         NULL,           NULL,           FileNamespace::Flag(0), 0,
            { "$(App.DefsPath)/", "$(App.DefsPath)/$(GamePlugin.Name)/", "$(App.DefsPath)/$(GamePlugin.Name)/$(Game.IdentityKey)/" }
        },
        { "Graphics",     "-gfxdir2",     "-gfxdir",      FileNamespace::Flag(0), 0,
            { "$(App.DataPath)/graphics/" }
        },
        { "Models",       "-modeldir2",   "-modeldir",    FileNamespace::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/models/", "$(App.DataPath)/$(GamePlugin.Name)/models/$(Game.IdentityKey)/" }
        },
        { "Sfx",          "-sfxdir2",     "-sfxdir",      FileNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/sfx/", "$(App.DataPath)/$(GamePlugin.Name)/sfx/$(Game.IdentityKey)/" }
        },
        { "Music",        "-musdir2",     "-musdir",      FileNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/music/", "$(App.DataPath)/$(GamePlugin.Name)/music/$(Game.IdentityKey)/" }
        },
        { "Textures",     "-texdir2",     "-texdir",      FileNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/textures/", "$(App.DataPath)/$(GamePlugin.Name)/textures/$(Game.IdentityKey)/" }
        },
        { "Flats",        "-flatdir2",    "-flatdir",     FileNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/flats/", "$(App.DataPath)/$(GamePlugin.Name)/flats/$(Game.IdentityKey)/" }
        },
        { "Patches",      "-patdir2",     "-patdir",      FileNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/patches/", "$(App.DataPath)/$(GamePlugin.Name)/patches/$(Game.IdentityKey)/" }
        },
        { "LightMaps",    "-lmdir2",      "-lmdir",       FileNamespace::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/lightmaps/" }
        },
        { "Fonts",        "-fontdir2",    "-fontdir",     FileNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/$(Game.IdentityKey)/" }
        },
        { 0, 0, 0, FileNamespace::Flag(0), 0, { 0 } }
    };

    createPackagesNamespace();

    // Setup the rest...
    struct namespacedef_s const* def = defs;
    for(int i = 0; defs[i].name; ++i, ++def)
    {
        FileNamespace& fnamespace = createFileNamespace(def->name, def->flags);

        int searchPathCount = 0;
        while(def->searchPaths[searchPathCount] && ++searchPathCount < namespacedef_max_searchpaths)
        {}

        for(int j = 0; j < searchPathCount; ++j)
        {
            de::Uri uri = de::Uri(def->searchPaths[j], RC_NULL);
            fnamespace.addSearchPath(FileNamespace::DefaultPaths, uri, def->searchPathFlags);
        }

        if(def->optOverridePath && CommandLine_CheckWith(def->optOverridePath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            fnamespace.addSearchPath(FileNamespace::OverridePaths, de::Uri::fromNativeDirPath(path), def->searchPathFlags);
            path = path / "$(Game.IdentityKey)";
            fnamespace.addSearchPath(FileNamespace::OverridePaths, de::Uri::fromNativeDirPath(path), def->searchPathFlags);
        }

        if(def->optFallbackPath && CommandLine_CheckWith(def->optFallbackPath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            fnamespace.addSearchPath(FileNamespace::FallbackPaths, de::Uri::fromNativeDirPath(path), def->searchPathFlags);
        }
    }
}

static void createResourceClasses()
{
    classes.push_back(new ResourceClass("RC_PACKAGE",       "Packages"));
    classes.push_back(new ResourceClass("RC_DEFINITION",    "Defs"));
    classes.push_back(new ResourceClass("RC_GRAPHIC",       "Graphics"));
    classes.push_back(new ResourceClass("RC_MODEL",         "Models"));
    classes.push_back(new ResourceClass("RC_SOUND",         "Sfx"));
    classes.push_back(new ResourceClass("RC_MUSIC",         "Music"));
    classes.push_back(new ResourceClass("RC_FONT",          "Fonts"));
}

static void createFileTypes()
{
    FileType* ftype;

    /*
     * Packages types:
     */
    ResourceClass& packageClass = F_ResourceClassByName("RC_PACKAGE");

    types.push_back(new ZipFileType());
    ftype = types.back();
    ftype->addKnownExtension(".pk3");
    ftype->addKnownExtension(".zip");
    packageClass.addFileType(ftype);

    types.push_back(new WadFileType());
    ftype = types.back();
    ftype->addKnownExtension(".wad");
    packageClass.addFileType(ftype);

    types.push_back(new FileType("FT_LMP", RC_PACKAGE)); ///< Treat lumps as packages so they are mapped to $App.DataPath.
    ftype = types.back();
    ftype->addKnownExtension(".lmp");

    /*
     * Definition types:
     */
    types.push_back(new FileType("FT_DED", RC_DEFINITION));
    ftype = types.back();
    ftype->addKnownExtension(".ded");
    F_ResourceClassByName("RC_DEFINITION").addFileType(ftype);

    /*
     * Graphic types:
     */
    ResourceClass& graphicClass = F_ResourceClassByName("RC_GRAPHIC");

    types.push_back(new FileType("FT_PNG", RC_GRAPHIC));
    ftype = types.back();
    ftype->addKnownExtension(".png");
    graphicClass.addFileType(ftype);

    types.push_back(new FileType("FT_TGA", RC_GRAPHIC));
    ftype = types.back();
    ftype->addKnownExtension(".tga");
    graphicClass.addFileType(ftype);

    types.push_back(new FileType("FT_JPG", RC_GRAPHIC));
    ftype = types.back();
    ftype->addKnownExtension(".jpg");
    graphicClass.addFileType(ftype);

    types.push_back(new FileType("FT_PCX", RC_GRAPHIC));
    ftype = types.back();
    ftype->addKnownExtension(".pcx");
    graphicClass.addFileType(ftype);

    /*
     * Model types:
     */
    ResourceClass& modelClass = F_ResourceClassByName("RC_MODEL");

    types.push_back(new FileType("FT_DMD", RC_MODEL));
    ftype = types.back();
    ftype->addKnownExtension(".dmd");
    modelClass.addFileType(ftype);

    types.push_back(new FileType("FT_MD2", RC_MODEL));
    ftype = types.back();
    ftype->addKnownExtension(".md2");
    modelClass.addFileType(ftype);

    /*
     * Sound types:
     */
    types.push_back(new FileType("FT_WAV", RC_SOUND));
    ftype = types.back();
    ftype->addKnownExtension(".wav");
    F_ResourceClassByName("RC_SOUND").addFileType(ftype);

    /*
     * Music types:
     */
    ResourceClass& musicClass = F_ResourceClassByName("RC_MUSIC");

    types.push_back(new FileType("FT_OGG", RC_MUSIC));
    ftype = types.back();
    ftype->addKnownExtension(".ogg");
    musicClass.addFileType(ftype);

    types.push_back(new FileType("FT_MP3", RC_MUSIC));
    ftype = types.back();
    ftype->addKnownExtension(".mp3");
    musicClass.addFileType(ftype);

    types.push_back(new FileType("FT_MOD", RC_MUSIC));
    ftype = types.back();
    ftype->addKnownExtension(".mod");
    musicClass.addFileType(ftype);

    types.push_back(new FileType("FT_MID", RC_MUSIC));
    ftype = types.back();
    ftype->addKnownExtension(".mid");
    musicClass.addFileType(ftype);

    /*
     * Font types:
     */
    types.push_back(new FileType("FT_DFN", RC_FONT));
    ftype = types.back();
    ftype->addKnownExtension(".dfn");
    F_ResourceClassByName("RC_FONT").addFileType(ftype);

    /*
     * Misc types:
     */
    types.push_back(new FileType("FT_DEH", RC_PACKAGE)); ///< Treat DeHackEd patches as packages so they are mapped to $App.DataPath.
    ftype = types.back();
    ftype->addKnownExtension(".deh");
}

void F_InitResourceLocator()
{
    if(inited) return;

    createResourceClasses();
    createFileTypes();
    createFileNamespaces();
    inited = true;
}

void F_ShutdownResourceLocator()
{
    if(!inited) return;

    DENG2_FOR_EACH(FileNamespaces, i, namespaces)
    {
        delete *i;
    }
    namespaces.clear();

    DENG2_FOR_EACH(FileTypes, i, types)
    {
        delete *i;
    }
    types.clear();

    DENG2_FOR_EACH(ResourceClasses, i, classes)
    {
        delete *i;
    }
    classes.clear();

    inited = false;
}

void F_ResetAllFileNamespaces()
{
    DENG2_FOR_EACH(FileNamespaces, i, namespaces)
    {
        (*i)->reset();
    }
}

FileNamespace* F_FileNamespaceByName(String name)
{
    if(!name.isEmpty())
    {
        DENG2_FOR_EACH(FileNamespaces, i, namespaces)
        {
            FileNamespace& fnamespace = **i;
            if(!fnamespace.name().compareWithoutCase(name))
                return &fnamespace;
        }
    }
    return 0; // Not found.
}

ResourceClass& F_ResourceClassByName(String name)
{
    if(!name.isEmpty())
    {
        DENG2_FOR_EACH_CONST(ResourceClasses, i, classes)
        {
            ResourceClass& rclass = **i;
            if(!rclass.name().compareWithoutCase(name))
                return rclass;
        }
    }
    return nullClass; // Not found.
}

FileType& F_FileTypeByName(String name)
{
    if(!name.isEmpty())
    {
        DENG2_FOR_EACH_CONST(FileTypes, i, types)
        {
            FileType& ftype = **i;
            if(!ftype.name().compareWithoutCase(name))
                return ftype;
        }
    }
    return nullType; // Not found.
}

FileType& F_GuessFileTypeFromFileName(String path)
{
    if(!path.isEmpty())
    {
        DENG2_FOR_EACH_CONST(FileTypes, i, types)
        {
            FileType& ftype = **i;
            if(ftype.fileNameIsKnown(path))
                return ftype;
        }
    }
    return nullType;
}

ResourceClass& F_ResourceClassById(resourceclassid_t id)
{
    if(!VALID_RESOURCECLASSID(id)) throw Error("F_ResourceClassById", String("Invalid id '%1'").arg(int(id)));
    return resourceClass(id);
}

FileTypes const& F_FileTypes()
{
    DENG_ASSERT(inited);
    return types;
}

FileNamespaces const& F_FileNamespaces()
{
    DENG_ASSERT(inited);
    return namespaces;
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
