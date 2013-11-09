/** @file resourcesystem.cpp  Resource subsystem.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "resource/resourcesystem.h"
#include <de/Log>

using namespace de;

DENG2_PIMPL(ResourceSystem)
{
    Textures textures;

    Instance(Public *i) : Base(i)
    {
        LOG_AS("ResourceSystem");

        LOG_MSG("Initializing Texture collection...");
        /// @note Order here defines the ambigious-URI search order.
        textures.createScheme("Sprites");
        textures.createScheme("Textures");
        textures.createScheme("Flats");
        textures.createScheme("Patches");
        textures.createScheme("System");
        textures.createScheme("Details");
        textures.createScheme("Reflections");
        textures.createScheme("Masks");
        textures.createScheme("ModelSkins");
        textures.createScheme("ModelReflectionSkins");
        textures.createScheme("Lightmaps");
        textures.createScheme("Flaremaps");
    }
};

ResourceSystem::ResourceSystem() : d(new Instance(this))
{}

void ResourceSystem::timeChanged(Clock const &)
{
    // Nothing to do.
}

Textures &ResourceSystem::textures()
{
    return d->textures;
}

void ResourceSystem::clearRuntimeTextureSchemes()
{
    d->textures.scheme("Flats").clear();
    d->textures.scheme("Textures").clear();
    d->textures.scheme("Patches").clear();
    d->textures.scheme("Sprites").clear();
    d->textures.scheme("Details").clear();
    d->textures.scheme("Reflections").clear();
    d->textures.scheme("Masks").clear();
    d->textures.scheme("ModelSkins").clear();
    d->textures.scheme("ModelReflectionSkins").clear();
    d->textures.scheme("Lightmaps").clear();
    d->textures.scheme("Flaremaps").clear();
}

void ResourceSystem::clearSystemTextureSchemes()
{
    d->textures.scheme("System").clear();
}
