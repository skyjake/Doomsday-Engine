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
#include <de/glshaderbank.h>
#include <de/gluniform.h>
#include <de/imagebank.h>
#include <de/vector.h>
#include <de/system.h>
#include "drawlists.h"
#include "configprofiles.h"
#include "projectedtexturedata.h"
#include "vectorlightdata.h"
#include "projectionlist.h"
#include "vectorlightlist.h"

class AngleClipper;
class IWorldRenderer;
class ModelRenderer;
class SkyDrawable;
namespace render { class Environment; }

/**
 * Renderer subsystems, draw lists, etc... @ingroup render
 */
class RenderSystem : public de::System
{
public:
    RenderSystem();

    // System.
    void timeChanged(const de::Clock &);

    ConfigProfiles &settings();
    ConfigProfiles &appearanceSettings();

    void glInit();
    void glDeinit();

    IWorldRenderer &world();
    de::GLShaderBank &shaders();
//    de::ImageBank &images();
    const de::GLUniform &uMapTime() const;
    de::GLUniform &uProjectionMatrix() const;
    de::GLUniform &uViewMatrix() const;
    render::Environment &environment();
    ModelRenderer &modelRenderer();
    SkyDrawable &sky();
    AngleClipper &angleClipper() const;

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
    void worldSystemMapChanged(world::Map &map);

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
    de::LoopResult forAllSurfaceProjections(de::duint listIdx, std::function<de::LoopResult (const ProjectedTextureData &)> func) const;

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
    de::LoopResult forAllVectorLights(de::duint listIdx, std::function<de::LoopResult (const VectorLightData &)> func);

public:
    /**
     * Register the console commands, variables, etc..., of this module.
     */
    static void consoleRegister();

private:
    DE_PRIVATE(d)
};

#endif  // CLIENT_RENDERSYSTEM_H
