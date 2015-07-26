/** @file resource/system.h
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

#ifndef LIBDOOMSDAY_RESOURCE_SYSTEM_H
#define LIBDOOMSDAY_RESOURCE_SYSTEM_H

#include <de/System>
#include <de/NativePath>
#include "resourceclass.h"

namespace res {

/**
 * Base class for the resource management subsystem.
 *
 * Singleton: there can only be one instance of the resource system at a time.
 */
class LIBDOOMSDAY_PUBLIC System : public de::System
{
public:
    /// An unknown resource class identifier was specified. @ingroup errors
    DENG2_ERROR(UnknownResourceClassError);

    static System &get();

public:
    System();

    // Systems observe the passage of time.
    void timeChanged(de::Clock const &) override;

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
     * Gets the path from "Config.resource.iwadFolder" and makes it the sole override
     * path for the Packages scheme.
     */
    void updateOverrideIWADPathFromConfig();

    /**
     * Returns the native path of the root of the saved session repository.
     */
    de::NativePath nativeSavePath() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace res

/**
 * Convenient method of returning a resource class from the application's global
 * resource system.
 *
 * @param className  Resource class name.
 */
LIBDOOMSDAY_PUBLIC ResourceClass &App_ResourceClass(de::String className);

/// @overload
LIBDOOMSDAY_PUBLIC ResourceClass &App_ResourceClass(resourceclassid_t classId);

#endif // LIBDOOMSDAY_RESOURCE_SYSTEM_H

