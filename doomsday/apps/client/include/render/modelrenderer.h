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

#include <de/ModelDrawable>
#include <de/ModelBank>
#include <de/GLState>

#include <QList>
#include <QMap>
#include <functional>

#include "vissprite.h"

/**
 * The model renderer prepares available model assets for drawing (using ModelDrawable),
 * and keeps the set of needed ModelDrawable instances in memory.
 *
 * ModelRenderer also owns the shaders for rendering models, and maintains the set of GL
 * uniforms for rendering models, including transformation and lighting data.
 *
 * @todo Consider renaming the class: the term "renderer" has the connotation of actually
 * performing rendering, while in practice the ModelDrawables will be drawing themselves.
 * This is the top-level class responsible for model assets and all their associated
 * data. Perhaps the class should be instead portrayed more as a specialized Bank. -jk
 *
 * @ingroup render
 */
class ModelRenderer
{
public:
    struct AnimSequence {
        de::String name;
        de::Record const *def;
        AnimSequence(de::String const &n, de::Record const &d)
            : name(n), def(&d) {}
    };
    typedef QList<AnimSequence> AnimSequences;
    struct StateAnims : public QMap<de::String, AnimSequences> {};

    struct AuxiliaryData : public de::ModelBank::IUserData
    {
        StateAnims animations;
        de::Matrix4f transformation;
        de::gl::Cull cull = de::gl::Back;
        bool autoscaleToThingHeight = true;
    };

public:
    ModelRenderer();

    void glInit();

    void glDeinit();

    /**
     * Provides access to the bank containing available drawable models.
     */
    de::ModelBank &bank();

    StateAnims const *animations(de::DotPath const &modelId) const;

    /**
     * Sets up the transformation matrices.
     *
     * @param relativeEyePos  Position of the eye in relation to object (in world space).
     * @param modelToLocal    Transformation from model space to the object's local space
     *                        (object's local frame in world space).
     * @param localToView     Transformation from local space to projected view space.
     */
    void setTransformation(de::Vector3f const &relativeEyePos,
                           de::Matrix4f const &modelToLocal,
                           de::Matrix4f const &localToView);

    void setEyeSpaceTransformation(de::Matrix4f const &modelToLocal,
                                   de::Matrix4f const &inverseLocal,
                                   de::Matrix4f const &localToView);

    void setAmbientLight(de::Vector3f const &ambientIntensity);

    void clearLights();

    void addLight(de::Vector3f const &direction, de::Vector3f const &intensity);

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
    static int identifierFromText(de::String const &text,
                           std::function<int (de::String const &)> resolver);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_MODELRENDERER_H
