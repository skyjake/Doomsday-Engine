/** @file rendersystem.h  Render subsystem.
 *
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include <functional>
#include <de/GLShaderBank>
#include <de/ImageBank>
#include <de/Vector>
#include <de/System>
#include "DrawLists"
#include "settingsregister.h"
#include "projectedtexturedata.h"
#include "vectorlightdata.h"

class AngleClipper;
class ModelRenderer;
class SkyDrawable;

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

/// @todo make private to RenderSystem
struct ProjectionList
{
    struct Node
    {
        Node *next, *nextUsed;
        ProjectedTextureData projection;
    };

    Node *head = nullptr;
    bool sortByLuma;       ///< @c true= Sort from brightest to darkest.

    static void init();
    static void rewind();

    inline ProjectionList &operator << (ProjectedTextureData &texp) { return add(texp); }

    ProjectionList &add(ProjectedTextureData &texp);

private:
    static Node *firstNode;
    static Node *cursorNode;

    static Node *newNode();
};

/// @todo make private to RenderSystem
struct VectorLightList
{
    struct Node
    {
        Node *next, *nextUsed;
        VectorLightData vlight;
    };

    Node *head = nullptr;

    static void init();
    static void rewind();

    inline VectorLightList &operator << (VectorLightData &texp) { return add(texp); }

    VectorLightList &add(VectorLightData &texp);

private:
    static Node *firstNode;
    static Node *cursorNode;

    static Node *newNode();
};

/**
 * Renderer subsystems, draw lists, etc... @ingroup render
 */
class RenderSystem : public de::System
{
public:
    RenderSystem();

    // System.
    void timeChanged(de::Clock const &);

    void glInit();
    void glDeinit();

    de::GLShaderBank &shaders();
    de::ImageBank &images();

    SettingsRegister &settings();
    SettingsRegister &appearanceSettings();

    AngleClipper &angleClipper() const;

    ModelRenderer &modelRenderer();

    SkyDrawable &sky();

    /**
     * Provides access to the central map geometry buffer.
     */
    Store &buffer();

    /**
     * Provides access to the DrawLists collection for conveniently writing geometry.
     */
    DrawLists &drawLists();

    /**
     * To be called manually, to clear all persistent data held by/for the draw lists
     * (e.g., during re-initialization).
     *
     * @todo Use a de::Observers based mechanism.
     */
    void clearDrawLists();

    /**
     * @todo Use a de::Observers based mechanism.
     */
    void worldSystemMapChanged(de::Map &map);

    /**
     * @todo Use a de::Observers based mechanism.
     */
    void beginFrame();

public:  // Texture => surface projection lists -----------------------------------

    /**
     * Find/create a new projection list.
     *
     * @param listIdx     Address holding the list index to retrieve. If the referenced
     *                    list index is non-zero return the associated list. Otherwise
     *                    allocate a new list and write it's index back to this address.
     *
     * @param sortByLuma  @c true= Maintain a luminosity sorted order (descending).
     *
     * @return  ProjectionList associated with the (possibly newly attributed) index.
     */
    ProjectionList &findSurfaceProjectionList(de::duint *listIdx, bool sortByLuma = false);

    /**
     * Iterate through the referenced list of texture => surface projections.
     *
     * @param listIdx  Unique identifier of the projection list to process.
     * @param func     Callback to make for each TexProjection.
     */
    de::LoopResult forAllSurfaceProjections(de::duint listIdx, std::function<de::LoopResult (ProjectedTextureData const &)> func) const;

public:  // VectorLight affection lists -------------------------------------------

    /**
     * Find/create a new vector light list.
     *
     * @param listIdx  Address holding the list index to retrieve. If the referenced
     *                 list index is non-zero return the associated list. Otherwise
     *                 allocate a new list and write it's index back to this address.
     *
     * @return  VectorLightList associated with the (possibly newly attributed) index.
     */
    VectorLightList &findVectorLightList(de::duint *listIdx);

    /**
     * Iterate through the referenced vector light list.
     *
     * @param listIdx  Unique identifier of the list to process.
     * @param func     Callback to make for each VectorLight.
     */
    de::LoopResult forAllVectorLights(de::duint listIdx, std::function<de::LoopResult (VectorLightData const &)> func);

public:
    /**
     * Register the console commands, variables, etc..., of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif  // CLIENT_RENDERSYSTEM_H
