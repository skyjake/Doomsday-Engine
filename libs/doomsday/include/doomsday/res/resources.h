/** @file resource/resources.h
 *
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_RESOURCES_H
#define LIBDOOMSDAY_RESOURCES_H

#include "../resourceclass.h"
#include <de/info.h>
#include <de/nativepath.h>
#include <de/pathtree.h>
#include <de/record.h>
#include <de/system.h>

namespace res
{
    class MapManifests;
    class ColorPalette;
    class ColorPalettes;
    class Textures;
    class AnimGroups;
    class Sprites;
}

/**
 * Base class for the resource management subsystem.
 *
 * Singleton: there can only be one instance of the resource system at a time.
 */
class LIBDOOMSDAY_PUBLIC Resources : public de::System
{
public:
    /// An unknown resource scheme was referenced. @ingroup errors
    DE_ERROR(UnknownSchemeError);

    /// The referenced resource was not found. @ingroup errors
    DE_ERROR(MissingResourceError);

    /// An unknown resource class identifier was specified. @ingroup errors
    DE_ERROR(UnknownResourceClassError);

    /// The referenced manifest was not found. @ingroup errors
    DE_ERROR(MissingResourceManifestError);

    static Resources &get();

public:
    Resources();

    // Systems observe the passage of time.
    void timeChanged(const de::Clock &) override;

    /**
     * Release all allocations, returning to the initial state.
     */
    virtual void clear();

    void clearAllResources();

    virtual void clearAllSystemResources();
    virtual void clearAllRuntimeResources();

    virtual void initSystemTextures();

    virtual void reloadAllResources();

    /**
     * Lookup a ResourceClass by symbolic @a name.
     */
    ResourceClass &resClass(const de::String& name);

    /**
     * Lookup a ResourceClass by @a id.
     * @todo Refactor away.
     */
    ResourceClass &resClass(resourceclassid_t id);

    /**
     * Returns the native path of the root of the saved session repository.
     */
    de::NativePath nativeSavePath() const;

    res::MapManifests &         mapManifests();
    const res::MapManifests &   mapManifests() const;

    res::ColorPalettes &        colorPalettes();
    const res::ColorPalettes &  colorPalettes() const;

    res::Textures &         textures();
    const res::Textures &   textures() const;

    res::AnimGroups &       animGroups();
    const res::AnimGroups & animGroups() const;

    res::Sprites &          sprites();
    const res::Sprites &    sprites() const;

    /**
     * Attempt to locate a music file referenced in the given @em Music @a definition.
     * Songs can be either in external files or non-MUS lumps.
     *
     * @note Lump based music is presently handled separately!
     *
     * @param musicDef  Music definition to find the music file for.
     *
     * @return  Absolute path to the music if found; otherwise a zero-length string.
     */
    de::String tryFindMusicFile(const de::Record &musicDef);

public:
    static de::String resolveSymbol(const de::String &symbol);

    static void consoleRegister();

private:
    DE_PRIVATE(d)
};

namespace res
{
    template <typename ManifestType>
    inline bool pathBeginsWithComparator(const ManifestType &manifest, void *context)
    {
        const auto *path = reinterpret_cast<de::Path *>(context);
        /// @todo Use PathTree::Node::compare()
        return manifest.path().toString().beginsWith(path->toString(), de::CaseInsensitive);
    }
}

/**
 * Convenient method of returning a resource class from the application's global
 * resource system.
 *
 * @param className  Resource class name.
 */
LIBDOOMSDAY_PUBLIC ResourceClass &App_ResourceClass(const de::String& className);

/// @overload
LIBDOOMSDAY_PUBLIC ResourceClass &App_ResourceClass(resourceclassid_t classId);

#endif // LIBDOOMSDAY_RESOURCES_H

