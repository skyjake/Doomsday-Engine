/** @file rend_model.h  Model renderer (v2.1, frame models).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef CLIENT_RENDER_MODEL_H
#define CLIENT_RENDER_MODEL_H

#include "resource/framemodeldef.h"
#include "render/modelrenderer.h"
#include "rend_main.h"

#include <de/vector.h>
#include <de/modelbank.h>
#include <de/modeldrawable.h>

class TextureVariantSpec;
namespace render { class StateAnimator; }
struct vissprite_t;

/// Absolute maximum number of vertices per submodel supported by this module.
#define RENDER_MAX_MODEL_VERTS  16192

/**
 * @todo Split this large inflexible structure into logical subcomponent pieces.
 * @ingroup render
 */
struct drawmodelparams_t
{
    // Animation, frame interpolation:
    FrameModelDef *mf;
    FrameModelDef *nextMF;
    float inter;
    dd_bool alwaysInterpolate;
    int id;                ///< For a unique skin offset.
    int selector;

    // Appearance:
    int flags;  ///< Mobj flags.
    int tmap;

    // Shiney texture mapping:
    float shineYawOffset;
    float shinePitchOffset;
    dd_bool shineTranslateWithViewerPos;
    dd_bool shinepspriteCoordSpace;       ///< Use the psprite coordinate space hack.
};

/// @ingroup render
struct drawmodel2params_t
{
    const struct mobj_s *object;
    const render::Model *model;
    const render::StateAnimator *animator;
};

DE_EXTERN_C de::dbyte useModels;
DE_EXTERN_C int modelLight;
DE_EXTERN_C int frameInter;
DE_EXTERN_C float modelAspectMod;
DE_EXTERN_C int mirrorHudModels;
//DE_EXTERN_C int modelShinyMultitex;
DE_EXTERN_C float modelSpinSpeed;
DE_EXTERN_C int maxModelDistance;
DE_EXTERN_C float rendModelLOD;
DE_EXTERN_C de::dbyte precacheSkins;

/**
 * Registers the console commands and variables used by this module.
 */
void Rend_ModelRegister();

/**
 * Initialize this module.
 */
void Rend_ModelInit();

/**
 * Shuts down this module.
 */
void Rend_ModelShutdown();

/**
 * Expand the render buffer to accommodate rendering models containing at most
 * this number of vertices.
 *
 * @note It is not actually necessary to call this. The vertex buffer will be
 *       enlarged automatically at render time to accommodate a given model so
 *       long as it contains less than RENDER_MAX_MODEL_VERTS. If not the model
 *       will simply not be rendered at all.
 *
 * @note Buffer reallocation is deferred until necessary, so repeatedly calling
 *       this routine during initialization is OK.
 *
 * @param numVertices  New maximum number of vertices we'll be required to handle.
 *
 * @return  @c true= successfully expanded. May fail if @a numVertices is larger
 *          than RENDER_MAX_MODEL_VERTS.
 */
bool Rend_ModelExpandVertexBuffers(de::duint numVertices);

/**
 * Lookup the texture specification for diffuse model skins.
 *
 * @param  noCompression  @c true= disable texture compression.
 * @return  Specification to be used when preparing such textures.
 */
const TextureVariantSpec &Rend_ModelDiffuseTextureSpec(bool noCompression);

/**
 * Lookup the texture specification for shiny model skins.
 *
 * @return  Specification to be used when preparing such textures.
 */
const TextureVariantSpec &Rend_ModelShinyTextureSpec();

/**
 * Render all the submodels of a model.
 */
void Rend_DrawModel(const vissprite_t &spr);

#endif  // CLIENT_RENDER_MODEL_H
