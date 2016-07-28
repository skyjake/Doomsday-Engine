/** @file resource/resources.cpp
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/resource/resources.h"
#include "doomsday/resource/animgroups.h"
#include "doomsday/resource/mapmanifests.h"
#include "doomsday/resource/colorpalettes.h"
#include "doomsday/resource/sprites.h"
#include "doomsday/resource/textures.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/DoomsdayApp"

#include <de/App>
#include <de/CommandLine>
#include <de/Config>

using namespace de;

static Resources *theResources = nullptr;

static String resolveUriSymbol(String const &symbol)
{
    if (!symbol.compare("App.DataPath", Qt::CaseInsensitive))
    {
        return "data";
    }
    else if (!symbol.compare("App.DefsPath", Qt::CaseInsensitive))
    {
        return "defs";
    }
    else if (!symbol.compare("Game.IdentityKey", Qt::CaseInsensitive))
    {
        if (DoomsdayApp::game().isNull())
        {
            /// @throw de::Uri::ResolveSymbolError  An unresolveable symbol was encountered.
            throw de::Uri::ResolveSymbolError("Resources::resolveUriSymbol",
                                              "Symbol 'Game' did not resolve (no game loaded)");
        }
        return DoomsdayApp::game().id();
    }
    else if (!symbol.compare("GamePlugin.Name", Qt::CaseInsensitive))
    {
        auto &gx = DoomsdayApp::plugins().gameExports();
        if (DoomsdayApp::game().isNull() || !gx.GetVariable)
        {
            /// @throw de::Uri::ResolveSymbolError  An unresolveable symbol was encountered.
            throw de::Uri::ResolveSymbolError("Resources::resolveUriSymbol",
                                              "Symbol 'GamePlugin' did not resolve (no game plugin loaded)");
        }
        return String(reinterpret_cast<char const *>(gx.GetVariable(DD_PLUGIN_NAME)));
    }
    else
    {
        /// @throw UnknownSymbolError  An unknown symbol was encountered.
        throw de::Uri::UnknownSymbolError("Resources::resolveUriSymbol",
                                          "Symbol '" + symbol + "' is unknown");
    }
}

DENG2_PIMPL(Resources)
{
    typedef QList<ResourceClass *> ResourceClasses;

    ResourceClasses     resClasses;
    NullResourceClass   nullResourceClass;
    NativePath          nativeSavePath;
    res::ColorPalettes  colorPalettes;
    res::MapManifests   mapManifests;
    res::Textures       textures;
    res::AnimGroups     animGroups;
    res::Sprites        sprites;

    Impl(Public *i)
        : Base(i)
        , nativeSavePath(App::app().nativeHomePath() / "savegames") // default
    {
        theResources = thisPublic;

        de::Uri::setResolverFunc(resolveUriSymbol);

        resClasses << new ResourceClass("RC_PACKAGE",    "Packages")
                   << new ResourceClass("RC_DEFINITION", "Defs")
                   << new ResourceClass("RC_GRAPHIC",    "Graphics")
                   << new ResourceClass("RC_MODEL",      "Models")
                   << new ResourceClass("RC_SOUND",      "Sfx")
                   << new ResourceClass("RC_MUSIC",      "Music")
                   << new ResourceClass("RC_FONT",       "Fonts");

        // Determine the root directory of the saved session repository.
        auto &cmdLine = App::commandLine();
        if (auto arg = cmdLine.check("-savedir", 1))
        {
            // Using a custom root save directory.
            cmdLine.makeAbsolutePath(arg.pos + 1);
            nativeSavePath = cmdLine.at(arg.pos + 1);
        }
    }

    ~Impl()
    {
        qDeleteAll(resClasses);
        textures.clear();

        theResources = nullptr;
    }
};

Resources::Resources() : d(new Impl(this))
{}

void Resources::timeChanged(Clock const &)
{
    // Nothing to do.
}

void Resources::clear()
{
    d->colorPalettes.clearAllColorPalettes();
}

Resources &Resources::get()
{
    DENG2_ASSERT(theResources);
    return *theResources;
}

ResourceClass &Resources::resClass(String name)
{
    if (!name.isEmpty())
    {
        foreach (ResourceClass *resClass, d->resClasses)
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
    throw UnknownResourceClassError("ResourceResources::toClass", QString("Invalid id '%1'").arg(int(id)));
}

NativePath Resources::nativeSavePath() const
{
    return d->nativeSavePath;
}

res::MapManifests &Resources::mapManifests()
{
    return d->mapManifests;
}

res::MapManifests const &Resources::mapManifests() const
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

res::Textures const &Resources::textures() const
{
    return d->textures;
}

res::AnimGroups &Resources::animGroups()
{
    return d->animGroups;
}

res::AnimGroups const &Resources::animGroups() const
{
    return d->animGroups;
}

res::Sprites &Resources::sprites()
{
    return d->sprites;
}

res::Sprites const &Resources::sprites() const
{
    return d->sprites;
}

ResourceClass &App_ResourceClass(String className)
{
    return Resources::get().resClass(className);
}

ResourceClass &App_ResourceClass(resourceclassid_t classId)
{
    return Resources::get().resClass(classId);
}
