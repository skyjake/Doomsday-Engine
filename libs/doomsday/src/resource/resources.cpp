/** @file resource/resources.cpp
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/resources.h"
#include "doomsday/res/animgroups.h"
#include "doomsday/res/mapmanifests.h"
#include "doomsday/res/colorpalettes.h"
#include "doomsday/res/sprites.h"
#include "doomsday/res/textures.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fs_util.h"
#include "doomsday/defs/music.h"
#include "doomsday/world/materials.h"
#include "doomsday/doomsdayapp.h"
#include "doomsday/savegames.h"
#include "doomsday/res/databundle.h"
#include "doomsday/res/doomsdaypackage.h"

#include <de/app.h>
#include <de/commandline.h>
#include <de/loop.h>
#include <de/packageloader.h>
#include <de/config.h>

using namespace de;
using namespace res;

static Resources *theResources = nullptr;

static String resolveUriSymbol(const String &symbol)
{
    if (!symbol.compare("App.DataPath", CaseInsensitive))
    {
        return "data";
    }
    else if (!symbol.compare("App.DefsPath", CaseInsensitive))
    {
        return "defs";
    }
    else if (!symbol.compare("Game.IdentityKey", CaseInsensitive))
    {
        if (DoomsdayApp::game().isNull())
        {
            /// @throw res::Uri::ResolveSymbolError  An unresolveable symbol was encountered.
            throw res::Uri::ResolveSymbolError("Resources::resolveUriSymbol",
                                              "Symbol 'Game' did not resolve (no game loaded)");
        }
        return DoomsdayApp::game().id();
    }
    else if (!symbol.compare("GamePlugin.Name", CaseInsensitive))
    {
        auto &gx = DoomsdayApp::plugins().gameExports();
        if (DoomsdayApp::game().isNull() || !gx.GetPointer)
        {
            /// @throw res::Uri::ResolveSymbolError  An unresolveable symbol was encountered.
            throw res::Uri::ResolveSymbolError("Resources::resolveUriSymbol",
                                              "Symbol 'GamePlugin' did not resolve (no game plugin loaded)");
        }
        return String(reinterpret_cast<const char *>(gx.GetPointer(DD_PLUGIN_NAME)));
    }
    else
    {
        /// @throw UnknownSymbolError  An unknown symbol was encountered.
        throw res::Uri::UnknownSymbolError("Resources::resolveUriSymbol",
                                          "Symbol '" + symbol + "' is unknown");
    }
}

DE_PIMPL(Resources)
, DE_OBSERVES(PackageLoader, Load)
, DE_OBSERVES(PackageLoader, Unload)
{
    typedef List<ResourceClass *> ResourceClasses;

    ResourceClasses     resClasses;
    NullResourceClass   nullResourceClass;
    NativePath          nativeSavePath;
    res::ColorPalettes  colorPalettes;
    res::MapManifests   mapManifests;
    res::Textures       textures;
    res::AnimGroups     animGroups;
    res::Sprites        sprites;
    Dispatch        deferredReset;

    Impl(Public *i)
        : Base(i)
        , nativeSavePath(App::app().nativeHomePath() / "savegames") // default
    {
        theResources = thisPublic;

        // Observe when resources need loading or unloading.
        App::packageLoader().audienceForLoad()   += this;
        App::packageLoader().audienceForUnload() += this;

        res::Uri::setResolverFunc(resolveUriSymbol);

        resClasses << new ResourceClass("RC_PACKAGE",    "Packages")
                   << new ResourceClass("RC_DEFINITION", "Defs")
                   << new ResourceClass("RC_GRAPHIC",    "Graphics")
                   << new ResourceClass("RC_MODEL",      "Models")
                   << new ResourceClass("RC_SOUND",      "Sfx")
                   << new ResourceClass("RC_MUSIC",      "Music")
                   << new ResourceClass("RC_FONT",       "Fonts");

        // Determine the root directory of the saved session repository.
        auto &cmdLine = App::commandLine();
        int pos = cmdLine.has("-savedir");
        if (pos > 0)
        {
            // Using a custom root save directory.
            cmdLine.makeAbsolutePath(pos + 1);
            String arg;
            if (cmdLine.getParameter("-savedir", arg))
            {
                nativeSavePath = arg;
            }
        }
    }

    ~Impl()
    {
        deleteAll(resClasses);
        textures.clear();

        theResources = nullptr;
    }

    void packageLoaded(const String &packageId)
    {
        maybeScheduleResourceReset(App::packageLoader().package(packageId), true);
    }

    void aboutToUnloadPackage(const String &packageId)
    {
        maybeScheduleResourceReset(App::packageLoader().package(packageId), false);
    }

    void maybeScheduleResourceReset(res::DoomsdayPackage ddPkg, bool loading)
    {
        if (!DoomsdayApp::isGameLoaded() || DoomsdayApp::isGameBeingChanged())
        {
            // Resources will be loaded when a game is loaded, so we don't need to
            // do anything at this time.
            return;
        }

        bool needReset = ddPkg.hasDefinitions();
        const File &sourceFile = ddPkg.sourceFile();

        if (const DataBundle *bundle = maybeAs<DataBundle>(sourceFile))
        {
            // DEH patches cannot be loaded/unloaded as such; they are simply
            // marked as loaded and applied all at once during a reset.
            if (bundle->format() == DataBundle::Dehacked)
            {
                needReset = true;
            }
            // We can try load the data file manually right now.
            // Data files are currently loaded via FS1.
            else if (   ( loading && File1::tryLoad(File1::LoadAsCustomFile, ddPkg.loadableUri()))
                     || (!loading && File1::tryUnload(ddPkg.loadableUri())))
            {
                needReset = true;
            }
        }

        if (!deferredReset && needReset)
        {
            deferredReset.enqueue([this] () { self().reloadAllResources(); });
        }
    }
};

Resources::Resources() : d(new Impl(this))
{}

void Resources::timeChanged(const Clock &)
{
    // Nothing to do.
}

void Resources::clear()
{
    d->colorPalettes.clearAllColorPalettes();
    d->animGroups   .clearAllAnimGroups();

    clearAllRuntimeResources();
}

void Resources::clearAllResources()
{
    clearAllRuntimeResources();
    clearAllSystemResources();
}

void Resources::clearAllSystemResources()
{
    textures().textureScheme("System").clear();
}

void Resources::clearAllRuntimeResources()
{
    textures().clearRuntimeTextures();
}

void Resources::initSystemTextures()
{
    LOG_AS("Resources");

    textures().declareSystemTexture("unknown", res::Uri("Graphics", "unknown"));
    textures().declareSystemTexture("missing", res::Uri("Graphics", "missing"));
}

void Resources::reloadAllResources()
{}

Resources &Resources::get()
{
    DE_ASSERT(theResources);
    return *theResources;
}

ResourceClass &Resources::resClass(const String& name)
{
    if (!name.isEmpty())
    {
        for (ResourceClass *resClass : d->resClasses)
        {
            if (!resClass->name().compareWithoutCase(name))
                return *resClass;
        }
    }
    return d->nullResourceClass; // Not found.
}

ResourceClass &Resources::resClass(resourceclassid_t id)
{
    if (id == RC_NULL) return d->nullResourceClass;
    if (VALID_RESOURCECLASSID(id))
    {
        return *d->resClasses.at(uint(id));
    }
    /// @throw UnknownResourceClass Attempted with an unknown id.
    throw UnknownResourceClassError("ResourceResources::toClass", stringf("Invalid id '%i'", int(id)));
}

NativePath Resources::nativeSavePath() const
{
    return d->nativeSavePath;
}

res::MapManifests &Resources::mapManifests()
{
    return d->mapManifests;
}

const res::MapManifests &Resources::mapManifests() const
{
    return d->mapManifests;
}

res::ColorPalettes &Resources::colorPalettes()
{
    return d->colorPalettes;
}

const res::ColorPalettes &Resources::colorPalettes() const
{
    return d->colorPalettes;
}

res::Textures &Resources::textures()
{
    return d->textures;
}

const res::Textures &Resources::textures() const
{
    return d->textures;
}

res::AnimGroups &Resources::animGroups()
{
    return d->animGroups;
}

const res::AnimGroups &Resources::animGroups() const
{
    return d->animGroups;
}

res::Sprites &Resources::sprites()
{
    return d->sprites;
}

const res::Sprites &Resources::sprites() const
{
    return d->sprites;
}

String Resources::tryFindMusicFile(const Record &definition)
{
    LOG_AS("Resources::tryFindMusicFile");

    defn::Music const music(definition);

    res::Uri songUri(music.gets("path"), RC_NULL);
    if (!songUri.path().isEmpty())
    {
        // All external music files are specified relative to the base path.
        String fullPath = App_BasePath() / songUri.path();
        if (F_Access(fullPath))
        {
            return fullPath;
        }

        LOG_AUDIO_WARNING("Music file \"%s\" not found (id '%s')")
                << songUri << music.gets("id");
    }

    // Try the resource locator.
    const String lumpName = music.gets("lumpName");
    if (!lumpName.isEmpty())
    {
        try
        {
            const String foundPath = App_FileSystem().findPath(res::Uri(lumpName, RC_MUSIC), RLF_DEFAULT,
                                                               App_ResourceClass(RC_MUSIC));
            return App_BasePath() / foundPath;  // Ensure the path is absolute.
        }
        catch (const FS1::NotFoundError &)
        {}  // Ignore this error.
    }
    return "";  // None found.
}

ResourceClass &App_ResourceClass(const String& className)
{
    return Resources::get().resClass(className);
}

ResourceClass &App_ResourceClass(resourceclassid_t classId)
{
    return Resources::get().resClass(classId);
}

/**
 * @param like             Map path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static dint printMapsIndex2(const Path &like, res::Uri::ComposeAsTextFlags composeUriFlags)
{
    res::MapManifests::Tree::FoundNodes found;
    Resources::get().mapManifests().allMapManifests()
            .findAll(found, res::pathBeginsWithComparator, const_cast<Path *>(&like));
    if (found.isEmpty()) return 0;

    //const bool printSchemeName = !(composeUriFlags & res::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known maps";
    //if (!printSchemeName && scheme)
    //    heading += " in scheme '" + scheme->name() + "'";
    if (!like.isEmpty())
        heading += " like \"" _E(b) + like.toString() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    std::sort(found.begin(), found.end(), comparePathTreeNodePathsAscending<res::MapManifest>);
//    const dint numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    dint idx = 0;
    for (res::MapManifest *mapManifest : found)
    {
        String info = Stringf(
            "%3i: " _E(1) "%s" _E(.), idx, mapManifest->description(composeUriFlags).c_str());

        LOG_RES_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

/**
 * @param scheme    Material subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Material path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printMaterialIndex2(world::MaterialScheme *scheme, const Path &like,
    res::Uri::ComposeAsTextFlags composeUriFlags)
{
    world::MaterialScheme::Index::FoundNodes found;
    if (scheme) // Consider resources in the specified scheme only.
    {
        scheme->index().findAll(found, res::pathBeginsWithComparator, const_cast<Path *>(&like));
    }
    else // Consider resources in any scheme.
    {
        world::Materials::get().forAllMaterialSchemes([&found, &like] (world::MaterialScheme &scheme)
        {
            scheme.index().findAll(found, res::pathBeginsWithComparator, const_cast<Path *>(&like));
            return LoopContinue;
        });
    }
    if (found.isEmpty()) return 0;

    const bool printSchemeName = !(composeUriFlags & res::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known materials";
    if (!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if (!like.isEmpty())
        heading += " like \"" _E(b) + like.toString() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    std::sort(found.begin(), found.end(), comparePathTreeNodePathsAscending<world::MaterialManifest>);
//    int const num/FoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    for (world::MaterialManifest *manifest : found)
    {
        String info = Stringf("%#i: %s%s" _E(.),
                        idx,
                        manifest->hasMaterial()? _E(1) : _E(2),
                        manifest->description(composeUriFlags).c_str());

        LOG_RES_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

/**
 * @param scheme    Texture subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Texture path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printTextureIndex2(res::TextureScheme *scheme, const Path &like,
                              res::Uri::ComposeAsTextFlags composeUriFlags)
{
    res::TextureScheme::Index::FoundNodes found;
    if (scheme)  // Consider resources in the specified scheme only.
    {
        scheme->index().findAll(found, res::pathBeginsWithComparator, const_cast<Path *>(&like));
    }
    else  // Consider resources in any scheme.
    {
        for (const auto &s : res::Textures::get().allTextureSchemes())
        {
            s.second->index().findAll(found, res::pathBeginsWithComparator, const_cast<Path *>(&like));
        }
    }
    if (found.isEmpty()) return 0;

    const bool printSchemeName = !(composeUriFlags & res::Uri::OmitScheme);

    // Print a heading.
    String heading = "Known textures";
    if (!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if (!like.isEmpty())
        heading += " like \"" _E(b) + like.toString() + _E(.) "\"";
    LOG_RES_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index key.
    std::sort(found.begin(), found.end(), comparePathTreeNodePathsAscending<res::TextureManifest>);
//    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    for (res::TextureManifest *manifest : found)
    {
        String info = Stringf("%3i: %s%s",
                        idx,
                        manifest->hasTexture()? _E(0) : _E(2),
                        manifest->description(composeUriFlags).c_str());

        LOG_RES_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

static void printMaterialIndex(const res::Uri &search,
    res::Uri::ComposeAsTextFlags flags = res::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = 0;

    // Collate and print results from all schemes?
    if (search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printMaterialIndex2(0/*any scheme*/, search.path(), flags & ~res::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if (world::Materials::get().isKnownMaterialScheme(search.scheme()))
    {
        printTotal = printMaterialIndex2(&world::Materials::get().materialScheme(search.scheme()),
                                         search.path(), flags | res::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        world::Materials::get().forAllMaterialSchemes([&search, &flags, &printTotal] (world::MaterialScheme &scheme)
        {
            int numPrinted = printMaterialIndex2(&scheme, search.path(), flags | res::Uri::OmitScheme);
            if (numPrinted)
            {
                LOG_MSG(_E(R));
                printTotal += numPrinted;
            }
            return LoopContinue;
        });
    }
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "material" : "materials in total");
}

static void printMapsIndex(const res::Uri &search,
    res::Uri::ComposeAsTextFlags flags = res::Uri::DefaultComposeAsTextFlags)
{
    int printTotal = printMapsIndex2(search.path(), flags | res::Uri::OmitScheme);
    LOG_RES_MSG(_E(R));
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "map" : "maps in total");
}

static void printTextureIndex(const res::Uri &search,
    res::Uri::ComposeAsTextFlags flags = res::Uri::DefaultComposeAsTextFlags)
{
    auto &textures = res::Textures::get();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if (search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printTextureIndex2(0/*any scheme*/, search.path(), flags & ~res::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if (textures.isKnownTextureScheme(search.scheme()))
    {
        printTotal = printTextureIndex2(&textures.textureScheme(search.scheme()),
                                        search.path(), flags | res::Uri::OmitScheme);
        LOG_RES_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        for (const auto &s : textures.allTextureSchemes())
        {
            int numPrinted = printTextureIndex2(s.second, search.path(), flags | res::Uri::OmitScheme);
            if (numPrinted)
            {
                LOG_RES_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_RES_MSG("Found " _E(b) "%i" _E(.) " %s") << printTotal << (printTotal == 1? "texture" : "textures in total");
}

static bool isKnownMaterialSchemeCallback(const String& name)
{
    return world::Materials::get().isKnownMaterialScheme(name);
}

static bool isKnownTextureSchemeCallback(const String& name)
{
    return res::Textures::get().isKnownTextureScheme(name);
}

/**
 * Print a list of all currently available maps and the location of the source
 * file which contains them.
 *
 * @todo Improve output: find common map id prefixes, find "suffixed" numerical
 * ranges, etc... (Do not assume *anything* about the formatting of map ids -
 * ZDoom allows the mod author to give a map any id they wish, which, is then
 * specified in MAPINFO when defining the map progression, clusters, etc...).
 */
D_CMD(ListMaps)
{
    DE_UNUSED(src);

    res::Uri search = res::Uri::fromUserInput(&argv[1], argc - 1);
    if (search.scheme().isEmpty()) search.setScheme("Maps");

    if (!search.scheme().isEmpty() && search.scheme().compareWithoutCase("Maps"))
    {
        LOG_RES_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printMapsIndex(search);
    return true;
}

D_CMD(ListMaterials)
{
    DE_UNUSED(src);

    res::Uri search = res::Uri::fromUserInput(&argv[1], argc - 1, &isKnownMaterialSchemeCallback);

    if (!search.scheme().isEmpty() &&
        !world::Materials::get().isKnownMaterialScheme(search.scheme()))
    {
        LOG_RES_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printMaterialIndex(search);
    return true;
}

D_CMD(ListTextures)
{
    DE_UNUSED(src);

    res::Uri search = res::Uri::fromUserInput(&argv[1], argc - 1, &isKnownTextureSchemeCallback);

    if (!search.scheme().isEmpty() &&
        !res::Textures::get().isKnownTextureScheme(search.scheme()))
    {
        LOG_RES_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    printTextureIndex(search);
    return true;
}

#ifdef DE_DEBUG
D_CMD(PrintMaterialStats)
{
    DE_UNUSED(src, argc, argv);

    LOG_MSG(_E(b) "Material Statistics:");
    world::Materials::get().forAllMaterialSchemes([] (world::MaterialScheme &scheme)
    {
        const world::MaterialScheme::Index &index = scheme.index();

        uint count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
                << scheme.name() << count << (count == 1? "material" : "materials");
        index.debugPrintHashDistribution();
        index.debugPrint();
        return LoopContinue;
    });
    return true;
}

D_CMD(PrintTextureStats)
{
    DE_UNUSED(src, argc, argv);

    LOG_MSG(_E(b) "Texture Statistics:");
    for (const auto &s : res::Textures::get().allTextureSchemes())
    {
        const res::TextureScheme::Index &index = s.second->index();

        const uint count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
            << s.second->name() << count << (count == 1? "texture" : "textures");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif // DE_DEBUG

void Resources::consoleRegister()
{
    C_CMD("listtextures",   "ss",   ListTextures)
    C_CMD("listtextures",   "s",    ListTextures)
    C_CMD("listtextures",   "",     ListTextures)

    C_CMD("listmaterials",  "ss",   ListMaterials)
    C_CMD("listmaterials",  "s",    ListMaterials)
    C_CMD("listmaterials",  "",     ListMaterials)

    C_CMD("listmaps",       "s",    ListMaps)
    C_CMD("listmaps",       "",     ListMaps)

#ifdef DE_DEBUG
    C_CMD("texturestats",   NULL,   PrintTextureStats)
    C_CMD("materialstats",  NULL,   PrintMaterialStats)
#endif

    SaveGames      ::consoleRegister();
    res  ::Texture ::consoleRegister();
    world::Material::consoleRegister();
}
