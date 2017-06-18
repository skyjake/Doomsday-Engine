/** @file modelrenderer.h  Model renderer.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_MODELRENDERER_H
#define DENG_CLIENT_MODELRENDERER_H

#include <de/Function>
#include <de/ModelDrawable>
#include <de/ModelBank>
#include <de/GLState>
#include <de/Timeline>
#include <de/MultiAtlas>

#include <QList>
#include <QMap>
#include <QBitArray>
#include <functional>

#include "model.h"

struct vissprite_t;
struct vispsprite_t;

namespace render { class ModelLoader; }

/**
 * The model renderer: draws 3D models representing map objects and psprites.
 *
 * ModelRenderer also does the following:
 * - prepares available model assets for drawing (using ModelDrawable), and keeps the
 *   set of needed ModelDrawable instances in memory
 * - owns the shaders for rendering models
 * - maintains the set of GL uniforms for rendering models, including transformation
 *   and lighting data
 * - orchestrates the rendering of individual models when it comes to rendering passes
 *   and GL state changes
 *
 * ModelRenderer does not perform the low-level OpenGL drawing calls -- those are done by
 * ModelDrawable.
 *
 * @ingroup render
 */
class ModelRenderer
{
public:
    ModelRenderer();

    void glInit();

    void glDeinit();

    render::ModelLoader &loader();

    render::ModelLoader const &loader() const;

    /**
     * Provides access to the bank containing available drawable models.
     */
    de::ModelBank &bank();

    render::Model::StateAnims const *animations(de::DotPath const &modelId) const;

    /**
     * Render a GL2 model.
     *
     * @param spr  Parameters for the draw operation (as a vissprite).
     */
    void render(vissprite_t const &spr);

    /**
     * Render a GL2 model representing a psprite.
     *
     * @param pspr        Parameters for the draw operation (as a vispsprite).
     * @param playerMobj  Player object. Light originating from this is ignored on the
     *                    psprite omdel.
     */
    void render(vispsprite_t const &pspr, struct mobj_s const *playerMobj);

public:
    static void initBindings(de::Binder &binder, de::Record &module);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_MODELRENDERER_H
