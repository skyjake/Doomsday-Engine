/** @file rendersystem.h  Render subsystem.
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

#ifndef CLIENT_RENDERSYSTEM_H
#define CLIENT_RENDERSYSTEM_H

#include <de/System>
#include <de/Vector>
#include <de/GLShaderBank>
#include <de/ImageBank>
#include "DrawLists"
#include "settingsregister.h"

/**
 * Geometry backing store (arrays).
 * @todo Replace with GLBuffer -ds
 */
struct Store
{
    /// Texture coordinate array indices.
    enum
    {
        TCA_MAIN, // Main texture.
        TCA_BLEND, // Blendtarget texture.
        TCA_LIGHT, // Dynlight texture.
        NUM_TEXCOORD_ARRAYS
    };

    de::Vector3f *posCoords;
    de::Vector2f *texCoords[NUM_TEXCOORD_ARRAYS];
    de::Vector4ub *colorCoords;

    Store();
    ~Store();

    void rewind();

    void clear();

    uint allocateVertices(uint count);

private:
    uint vertCount, vertMax;
};

/**
 * Renderer subsystems, draw lists, etc... @ingroup render
 */
class RenderSystem : public de::System
{
public:
    RenderSystem();

    de::GLShaderBank &shaders();
    de::ImageBank &images();

    SettingsRegister &settings();
    SettingsRegister &appearanceSettings();

    /**
     * Provides access to the central map geometry buffer.
     */
    Store &buffer();

    void clearDrawLists();

    void resetDrawLists();

    DrawLists &drawLists();

    // System.
    void timeChanged(de::Clock const &);

public:
    /**
     * Register the console commands, variables, etc..., of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_RENDERSYSTEM_H
