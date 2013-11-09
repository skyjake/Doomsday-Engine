/** @file resourcesystem.h  Resource subsystem.
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

#ifndef DENG_RESOURCESYSTEM_H
#define DENG_RESOURCESYSTEM_H

#include <de/System>
#include "Textures"

/**
 * Logical resources; materials, packages, textures, etc... @ingroup resource
 */
class ResourceSystem : public de::System
{
public:
    ResourceSystem();

    de::Textures &textures();

    void clearRuntimeTextureSchemes();

    void clearSystemTextureSchemes();

    // System.
    void timeChanged(de::Clock const &);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_RESOURCESYSTEM_H
