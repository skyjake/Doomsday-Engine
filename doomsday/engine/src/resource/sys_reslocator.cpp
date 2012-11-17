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

class ZipResourceType : public de::FileResourceType
{
public:
    ZipResourceType() : FileResourceType("RT_ZIP", RC_PACKAGE)
    {}

    de::File1* interpret(de::FileHandle& hndl, String path, FileInfo const& info) const
    {
        if(Zip::recognise(hndl))
        {
            LOG_AS("ZipResourceType");
            LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
            return new Zip(hndl, path, info);
        }
        return 0;
    }
};

class WadResourceType : public de::FileResourceType
{
public:
    WadResourceType() : FileResourceType("RT_WAD", RC_PACKAGE)
    {}

    de::File1* interpret(de::FileHandle& hndl, String path, FileInfo const& info) const
    {
        if(Wad::recognise(hndl))
        {
            LOG_AS("WadResourceType");
            LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
            return new Wad(hndl, path, info);
        }
        return 0;
    }
};

static bool inited = false;

static NullResourceClass nullClass;
static NullResourceType nullType;

static ResourceTypes types;
static ResourceClasses classes;
static ResourceNamespaces namespaces;

static inline ResourceClass& resourceClass(resourceclassid_t id)
{
    if(id == RC_NULL) return nullClass;
    if(!VALID_RESOURCE_CLASSID(id)) throw Error("resourceClass", String("Invalid id %1").arg(id));
    return *classes[uint(id)];
}

/**
 * @param name      Unique symbolic name of this namespace. Must be at least
 *                  @c RESOURCENAMESPACE_MINNAMELENGTH characters long.
 * @param flags     @ref ResourceNamespace::Flag
 */
static ResourceNamespace& createResourceNamespace(String name, ResourceNamespace::Flags flags = 0)
{
    DENG_ASSERT(name.length() >= RESOURCENAMESPACE_MINNAMELENGTH);
    namespaces.push_back(new ResourceNamespace(name, flags));
    return *namespaces.back();
}

static bool findResourceInNamespace(ResourceNamespace& rnamespace, de::Uri const& searchPath,
    ddstring_t* foundPath)
{
    if(searchPath.isEmpty()) return 0;

    LOG_TRACE("Using namespace '%s'...") << rnamespace.name();

    // Ensure the namespace is up to date.
    rnamespace.rebuild();

    // A resource name is the file name sans extension.
    String name = searchPath.firstPathNode().toString().fileNameWithoutExtension();

    // Perform the search.
    ResourceNamespace::ResourceList foundResources;
    if(rnamespace.findAll(name, foundResources))
    {
        // There is at least one name-matched (perhaps partially) resource.
        DENG2_FOR_EACH_CONST(ResourceNamespace::ResourceList, i, foundResources)
        {
            PathTree::Node& node = **i;
            if(!node.comparePath(searchPath, PCF_NO_BRANCH))
            {
                // This is the resource we are looking for.
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

static bool findResourceFile(de::Uri const& searchPath, ddstring_t* foundPath)
{
    try
    {
        de::File1& file = App_FileSystem()->find(searchPath.compose());
        // Does the caller want to know the matched path?
        if(foundPath)
        {
            QByteArray searchPathUtf8 = file.composePath().toUtf8();
            Str_Set(foundPath, searchPathUtf8.constData());
            F_PrependBasePath(foundPath, foundPath);
        }
        return true;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.
    return false;
}

static bool findResource3(ResourceNamespace* rnamespace, de::Uri const& searchPath,
    ddstring_t* foundPath)
{
    // Is there a namespace we should use?
    if(rnamespace)
    {
        return findResourceInNamespace(*rnamespace, searchPath, foundPath);
    }
    return findResourceFile(searchPath, foundPath);
}

static bool findResource2(int flags, resourceclassid_t classId, String searchPath,
    ddstring_t* foundPath, ResourceNamespace* rnamespace)
{
    if(searchPath.isEmpty()) return false;

    // If an extension was specified, first look for resources of the same type.
    String ext = searchPath.fileNameExtension();
    if(!ext.isEmpty() && ext.compare(".*"))
    {
        if(findResource3(rnamespace, de::Uri(searchPath, RC_NULL), foundPath)) return true;

        // If we are looking for a particular resource type, get out of here.
        if(flags & RLF_MATCH_EXTENSION) return false;
    }

    ResourceClass const& rclass = resourceClass(classId);
    if(!rclass.resourceTypeCount()) return false;

    /*
     * Try some different name patterns (i.e., resource types) known to us.
     */

    // Create a copy of the searchPath minus file extension.
    String path2 = searchPath.fileNamePath() / searchPath.fileNameWithoutExtension();

    path2.reserve(path2.length() + 5 /*max expected extension length*/);

    DENG2_FOR_EACH_CONST(ResourceClass::Types, typeIt, rclass.resourceTypes())
    {
        DENG2_FOR_EACH_CONST(QStringList, i, (*typeIt)->knownFileNameExtensions())
        {
            String const& ext = *i;
            if(findResource3(rnamespace, de::Uri(path2 + ext, RC_NULL), foundPath))
            {
                return true;
            }
        }
    };

    return false; // Not found.
}

static bool findResource(resourceclassid_t classId, de::Uri const& searchPath,
    ddstring_t* foundPath, int flags, String optionalSuffix = "")
{
    DENG_ASSERT(classId == RC_UNKNOWN || VALID_RESOURCE_CLASSID(classId));

    LOG_AS("findResource");

    if(searchPath.isEmpty()) return false;

    try
    {
        String const& resolvedPath = searchPath.resolved();

        // Is a namespace specified?
        ResourceNamespace* rnamespace = F_ResourceNamespaceByName(searchPath.schemeCStr());

        // First try with the optional suffix.
        if(!optionalSuffix.isEmpty())
        {
            String resolvedPath2 = resolvedPath.fileNamePath()
                                 / resolvedPath.fileNameWithoutExtension() + optionalSuffix + resolvedPath.fileNameExtension();

            if(findResource2(flags, classId, resolvedPath2, foundPath, rnamespace))
                return true;
        }

        // Try without a suffix.
        return findResource2(flags, classId, resolvedPath, foundPath, rnamespace);
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
    ResourceNamespace& rnamespace = createResourceNamespace("Packages");

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
        rnamespace.addSearchPath(ResourceNamespace::DefaultPaths, de::Uri::fromNativeDirPath(path), SPF_NO_DESCEND);
        LOG_INFO("Using paths.iwaddir: %s") << path.pretty();
    }
#endif

    // Add the path from the DOOMWADDIR environment variable.
    if(!CommandLine_Check("-nodoomwaddir") && getenv("DOOMWADDIR"))
    {
        NativePath path = de::App::app().commandLine().startupPath() / getenv("DOOMWADDIR");
        rnamespace.addSearchPath(ResourceNamespace::DefaultPaths, de::Uri::fromNativeDirPath(path), SPF_NO_DESCEND);
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
            rnamespace.addSearchPath(ResourceNamespace::DefaultPaths, de::Uri::fromNativeDirPath(path), SPF_NO_DESCEND);
            LOG_INFO("Using DOOMWADPATH: %s") << path.pretty();
        }

#undef SEP_CHAR
    }

    rnamespace.addSearchPath(ResourceNamespace::DefaultPaths, de::Uri("$(App.DataPath)/", RC_NULL), SPF_NO_DESCEND);
    rnamespace.addSearchPath(ResourceNamespace::DefaultPaths, de::Uri("$(App.DataPath)/$(GamePlugin.Name)/", RC_NULL), SPF_NO_DESCEND);
}

static void createResourceNamespaces()
{
    int const namespacedef_max_searchpaths = 5;
    struct namespacedef_s {
        char const* name;
        char const* optOverridePath;
        char const* optFallbackPath;
        ResourceNamespace::Flags flags;
        int searchPathFlags; /// @see searchPathFlags
        /// Priority is right to left.
        char const* searchPaths[namespacedef_max_searchpaths];
    } defs[] = {
        { "Defs",         NULL,           NULL,           ResourceNamespace::Flag(0), 0,
            { "$(App.DefsPath)/", "$(App.DefsPath)/$(GamePlugin.Name)/", "$(App.DefsPath)/$(GamePlugin.Name)/$(Game.IdentityKey)/" }
        },
        { "Graphics",     "-gfxdir2",     "-gfxdir",      ResourceNamespace::Flag(0), 0,
            { "$(App.DataPath)/graphics/" }
        },
        { "Models",       "-modeldir2",   "-modeldir",    ResourceNamespace::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/models/", "$(App.DataPath)/$(GamePlugin.Name)/models/$(Game.IdentityKey)/" }
        },
        { "Sfx",          "-sfxdir2",     "-sfxdir",      ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/sfx/", "$(App.DataPath)/$(GamePlugin.Name)/sfx/$(Game.IdentityKey)/" }
        },
        { "Music",        "-musdir2",     "-musdir",      ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/music/", "$(App.DataPath)/$(GamePlugin.Name)/music/$(Game.IdentityKey)/" }
        },
        { "Textures",     "-texdir2",     "-texdir",      ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/textures/", "$(App.DataPath)/$(GamePlugin.Name)/textures/$(Game.IdentityKey)/" }
        },
        { "Flats",        "-flatdir2",    "-flatdir",     ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/flats/", "$(App.DataPath)/$(GamePlugin.Name)/flats/$(Game.IdentityKey)/" }
        },
        { "Patches",      "-patdir2",     "-patdir",      ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/patches/", "$(App.DataPath)/$(GamePlugin.Name)/patches/$(Game.IdentityKey)/" }
        },
        { "LightMaps",    "-lmdir2",      "-lmdir",       ResourceNamespace::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/lightmaps/" }
        },
        { "Fonts",        "-fontdir2",    "-fontdir",     ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/$(Game.IdentityKey)/" }
        },
        { 0, 0, 0, ResourceNamespace::Flag(0), 0, { 0 } }
    };

    createPackagesNamespace();

    // Setup the rest...
    struct namespacedef_s const* def = defs;
    for(int i = 0; defs[i].name; ++i, ++def)
    {
        ResourceNamespace& rnamespace = createResourceNamespace(def->name, def->flags);

        int searchPathCount = 0;
        while(def->searchPaths[searchPathCount] && ++searchPathCount < namespacedef_max_searchpaths)
        {}

        for(int j = 0; j < searchPathCount; ++j)
        {
            de::Uri uri = de::Uri(def->searchPaths[j], RC_NULL);
            rnamespace.addSearchPath(ResourceNamespace::DefaultPaths, uri, def->searchPathFlags);
        }

        if(def->optOverridePath && CommandLine_CheckWith(def->optOverridePath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            rnamespace.addSearchPath(ResourceNamespace::OverridePaths, de::Uri::fromNativeDirPath(path), def->searchPathFlags);
            path = path / "$(Game.IdentityKey)";
            rnamespace.addSearchPath(ResourceNamespace::OverridePaths, de::Uri::fromNativeDirPath(path), def->searchPathFlags);
        }

        if(def->optFallbackPath && CommandLine_CheckWith(def->optFallbackPath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            rnamespace.addSearchPath(ResourceNamespace::FallbackPaths, de::Uri::fromNativeDirPath(path), def->searchPathFlags);
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

static void createResourceTypes()
{
    ResourceType* rtype;

    /*
     * Packages types:
     */
    ResourceClass& packageClass = F_ResourceClassByName("RC_PACKAGE");

    types.push_back(new ZipResourceType());
    rtype = types.back();
    rtype->addKnownExtension(".pk3");
    rtype->addKnownExtension(".zip");
    packageClass.addResourceType(rtype);

    types.push_back(new WadResourceType());
    rtype = types.back();
    rtype->addKnownExtension(".wad");
    packageClass.addResourceType(rtype);

    types.push_back(new ResourceType("RT_LMP", RC_PACKAGE)); ///< Treat lumps as packages so they are mapped to $App.DataPath.
    rtype = types.back();
    rtype->addKnownExtension(".lmp");

    /*
     * Definition types:
     */
    types.push_back(new ResourceType("RT_DED", RC_DEFINITION));
    rtype = types.back();
    rtype->addKnownExtension(".ded");
    F_ResourceClassByName("RC_DEFINITION").addResourceType(rtype);

    /*
     * Graphic types:
     */
    ResourceClass& graphicClass = F_ResourceClassByName("RC_GRAPHIC");

    types.push_back(new ResourceType("RT_PNG", RC_GRAPHIC));
    rtype = types.back();
    rtype->addKnownExtension(".png");
    graphicClass.addResourceType(rtype);

    types.push_back(new ResourceType("RT_TGA", RC_GRAPHIC));
    rtype = types.back();
    rtype->addKnownExtension(".tga");
    graphicClass.addResourceType(rtype);

    types.push_back(new ResourceType("RT_JPG", RC_GRAPHIC));
    rtype = types.back();
    rtype->addKnownExtension(".jpg");
    graphicClass.addResourceType(rtype);

    types.push_back(new ResourceType("RT_PCX", RC_GRAPHIC));
    rtype = types.back();
    rtype->addKnownExtension(".pcx");
    graphicClass.addResourceType(rtype);

    /*
     * Model types:
     */
    ResourceClass& modelClass = F_ResourceClassByName("RC_MODEL");

    types.push_back(new ResourceType("RT_DMD", RC_MODEL));
    rtype = types.back();
    rtype->addKnownExtension(".dmd");
    modelClass.addResourceType(rtype);

    types.push_back(new ResourceType("RT_MD2", RC_MODEL));
    rtype = types.back();
    rtype->addKnownExtension(".md2");
    modelClass.addResourceType(rtype);

    /*
     * Sound types:
     */
    types.push_back(new ResourceType("RT_WAV", RC_SOUND));
    rtype = types.back();
    rtype->addKnownExtension(".wav");
    F_ResourceClassByName("RC_SOUND").addResourceType(rtype);

    /*
     * Music types:
     */
    ResourceClass& musicClass = F_ResourceClassByName("RC_MUSIC");

    types.push_back(new ResourceType("RT_OGG", RC_MUSIC));
    rtype = types.back();
    rtype->addKnownExtension(".ogg");
    musicClass.addResourceType(rtype);

    types.push_back(new ResourceType("RT_MP3", RC_MUSIC));
    rtype = types.back();
    rtype->addKnownExtension(".mp3");
    musicClass.addResourceType(rtype);

    types.push_back(new ResourceType("RT_MOD", RC_MUSIC));
    rtype = types.back();
    rtype->addKnownExtension(".mod");
    musicClass.addResourceType(rtype);

    types.push_back(new ResourceType("RT_MID", RC_MUSIC));
    rtype = types.back();
    rtype->addKnownExtension(".mid");
    musicClass.addResourceType(rtype);

    /*
     * Font types:
     */
    types.push_back(new ResourceType("RT_DFN", RC_FONT));
    rtype = types.back();
    rtype->addKnownExtension(".dfn");
    F_ResourceClassByName("RC_FONT").addResourceType(rtype);

    /*
     * Misc types:
     */
    types.push_back(new ResourceType("RT_DEH", RC_PACKAGE)); ///< Treat DeHackEd patches as packages so they are mapped to $App.DataPath.
    rtype = types.back();
    rtype->addKnownExtension(".deh");
}

void F_InitResourceLocator()
{
    if(inited) return;

    createResourceClasses();
    createResourceTypes();
    createResourceNamespaces();
    inited = true;
}

void F_ShutdownResourceLocator()
{
    if(!inited) return;

    DENG2_FOR_EACH(ResourceNamespaces, i, namespaces)
    {
        delete *i;
    }
    namespaces.clear();

    DENG2_FOR_EACH(ResourceTypes, i, types)
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

void F_ResetAllResourceNamespaces()
{
    DENG2_FOR_EACH(ResourceNamespaces, i, namespaces)
    {
        (*i)->reset();
    }
}

ResourceNamespace* F_ResourceNamespaceByName(String name)
{
    if(!name.isEmpty())
    {
        DENG2_FOR_EACH(ResourceNamespaces, i, namespaces)
        {
            ResourceNamespace& rnamespace = **i;
            if(!rnamespace.name().compareWithoutCase(name))
                return &rnamespace;
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

ResourceType& F_ResourceTypeByName(String name)
{
    if(!name.isEmpty())
    {
        DENG2_FOR_EACH_CONST(ResourceTypes, i, types)
        {
            ResourceType& rtype = **i;
            if(!rtype.name().compareWithoutCase(name))
                return rtype;
        }
    }
    return nullType; // Not found.
}

ResourceType& F_GuessResourceTypeFromFileName(String path)
{
    if(!path.isEmpty())
    {
        DENG2_FOR_EACH_CONST(ResourceTypes, i, types)
        {
            ResourceType& rtype = **i;
            if(rtype.fileNameIsKnown(path))
                return rtype;
        }
    }
    return nullType;
}

ResourceClass* F_ResourceClassById(resourceclassid_t id)
{
    if(!VALID_RESOURCE_CLASSID(id)) return 0;
    return &resourceClass(id);
}

ResourceTypes const& F_ResourceTypes()
{
    DENG_ASSERT(inited);
    return types;
}

ResourceNamespaces const& F_ResourceNamespaces()
{
    DENG_ASSERT(inited);
    return namespaces;
}

boolean F_FindResource4(resourceclassid_t classId, uri_s const* searchPath,
    ddstring_t* foundPath, int flags, char const* optionalSuffix)
{
    DENG_ASSERT(searchPath);
    return findResource(classId, reinterpret_cast<de::Uri const&>(*searchPath), foundPath, flags, optionalSuffix);
}

boolean F_FindResource3(resourceclassid_t classId, uri_s const* searchPath,
    ddstring_t* foundPath, int flags)
{
    return F_FindResource4(classId, searchPath, foundPath, flags, NULL);
}

boolean F_FindResource2(resourceclassid_t classId, uri_s const* searchPath,
    ddstring_t* foundPath)
{
    return F_FindResource3(classId, searchPath, foundPath, RLF_DEFAULT);
}

boolean F_FindResource(resourceclassid_t classId, uri_s const* searchPath)
{
    return F_FindResource2(classId, searchPath, NULL);
}

uint F_FindResourceFromList(resourceclassid_t classId, char const* searchPaths,
    ddstring_t* foundPath, int flags, char const* optionalSuffix)
{
    if(!searchPaths || !searchPaths[0]) return 0;

    QStringList paths = String(searchPaths).split(';', QString::SkipEmptyParts);
    int pathIndex = 0;
    for(QStringList::const_iterator i = paths.constBegin(); i != paths.constEnd(); ++i, ++pathIndex)
    {
        de::Uri searchPath = de::Uri(*i, classId);
        if(findResource(classId, searchPath, foundPath, flags, optionalSuffix))
        {
            return pathIndex + 1; // 1-based index.
        }
    }

    return 0; // Not found.
}
