/** @file modelrenderer.h  Model renderer.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/Scheduler>

#include <QList>
#include <QMap>
#include <QBitArray>
#include <functional>

struct vissprite_t;
struct vispsprite_t;

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
    /**
     * Animation sequence definition.
     */
    struct AnimSequence {
        de::String name;        ///< Name of the sequence.
        de::Record const *def;  ///< Record describing the sequence (in asset metadata).
        de::Scheduler *timeline = nullptr; ///< Script timeline (owned).
        de::String sharedTimeline; ///< Name of shared timeline (if specified).

        AnimSequence(de::String const &n, de::Record const &d);
    };
    typedef QList<AnimSequence> AnimSequences;
    struct StateAnims : public QMap<de::String, AnimSequences> {};

    /**
     * Auxiliary data stored in the model bank.
     */
    struct AuxiliaryData : public de::ModelBank::IUserData
    {
        bool autoscaleToThingHeight = true;
        de::Matrix4f transformation;
        de::gl::Cull cull = de::gl::Back;
        de::ModelDrawable::Passes passes;

        StateAnims animations;

        /// Shared timelines (not sequence-specific). Owned.
        QMap<de::String, de::Scheduler *> timelines;

        ~AuxiliaryData();
    };

    DENG2_ERROR(DefinitionError);
    DENG2_ERROR(TextureMappingError);

public:
    ModelRenderer();

    void glInit();

    void glDeinit();

    /**
     * Provides access to the bank containing available drawable models.
     */
    de::ModelBank &bank();

    AuxiliaryData const *auxiliaryData(de::DotPath const &modelId) const;

    StateAnims const *animations(de::DotPath const &modelId) const;

    /**
     * Render a GL2 model.
     *
     * @param spr  Parameters for the draw operation (as a vissprite).
     */
    void render(vissprite_t const &spr);

    /**
     * Render a GL2 model representing a psprite.
     *
     * @param pspr  Parameters for the draw operation (as a vispsprite).
     */
    void render(vispsprite_t const &pspr);

public:
    static void initBindings(de::Binder &binder, de::Record &module);

    static int identifierFromText(de::String const &text,
                                  std::function<int (de::String const &)> resolver);

private:
    DENG2_PRIVATE(d)
};

typedef ModelRenderer::AuxiliaryData ModelAuxiliaryData;

#endif // DENG_CLIENT_MODELRENDERER_H
