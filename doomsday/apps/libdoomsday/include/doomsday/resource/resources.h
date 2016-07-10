/** @file resource/resources.h
 *
 * @authors Copyright © 2013-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#include "resourceclass.h"
#include <de/NativePath>
#include <de/PathTree>
#include <de/System>
#include <de/Info>

namespace res
{
    class MapManifests;
    class ColorPalettes;
    class Textures;
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
    DENG2_ERROR(UnknownSchemeError);

    /// The referenced resource was not found. @ingroup errors
    DENG2_ERROR(MissingResourceError);

    /// An unknown resource class identifier was specified. @ingroup errors
    DENG2_ERROR(UnknownResourceClassError);

    /// The referenced manifest was not found. @ingroup errors
    DENG2_ERROR(MissingResourceManifestError);

    static Resources &get();

public:
    Resources();

    // Systems observe the passage of time.
    void timeChanged(de::Clock const &) override;

    /**
     * Release all allocations, returning to the initial state.
     */
    virtual void clear();

    virtual void clearAllMaterialSchemes();

    /**
     * Lookup a ResourceClass by symbolic @a name.
     */
    ResourceClass &resClass(de::String name);

    /**
     * Lookup a ResourceClass by @a id.
     * @todo Refactor away.
     */
    ResourceClass &resClass(resourceclassid_t id);

    /**
     * Returns the native path of the root of the saved session repository.
     */
    de::NativePath nativeSavePath() const;

    res::MapManifests &       mapManifests();
    res::MapManifests const & mapManifests() const;

    res::ColorPalettes &       colorPalettes();
    res::ColorPalettes const & colorPalettes() const;

    res::Textures &       textures();
    res::Textures const & textures() const;

private:
    DENG2_PRIVATE(d)
};

/**
 * Convenient method of returning a resource class from the application's global
 * resource system.
 *
 * @param className  Resource class name.
 */
LIBDOOMSDAY_PUBLIC ResourceClass &App_ResourceClass(de::String className);

/// @overload
LIBDOOMSDAY_PUBLIC ResourceClass &App_ResourceClass(resourceclassid_t classId);

#endif // LIBDOOMSDAY_RESOURCES_H

