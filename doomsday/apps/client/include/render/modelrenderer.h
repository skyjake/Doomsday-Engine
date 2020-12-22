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

#ifndef DE_CLIENT_MODELRENDERER_H
#define DE_CLIENT_MODELRENDERER_H

#include "model.h"

#include <de/scripting/function.h>
#include <de/scripting/timeline.h>
#include <de/modeldrawable.h>
#include <de/modelbank.h>
#include <de/glstate.h>
#include <de/multiatlas.h>

#include <functional>

struct vissprite_t;
struct vispsprite_t;

namespace render { class ModelLoader; }

extern float weaponFixedFOV; // cvar

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

    render::ModelLoader &      loader();
    const render::ModelLoader &loader() const;

    /**
     * Provides access to the bank containing available drawable models.
     */
    de::ModelBank &bank();

    const render::Model::StateAnims *animations(const de::DotPath &modelId) const;

    /**
     * Render a GL2 model.
     *
     * @param spr  Parameters for the draw operation (as a vissprite).
     */
    void render(const vissprite_t &spr);

    /**
     * Render a GL2 model representing a psprite.
     *
     * @param pspr        Parameters for the draw operation (as a vispsprite).
     * @param playerMobj  Player object. Light originating from this is ignored on the
     *                    psprite omdel.
     */
    void render(const vispsprite_t &pspr, const struct mobj_s *playerMobj);

public:
    static void initBindings(de::Binder &binder, de::Record &module);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_MODELRENDERER_H
