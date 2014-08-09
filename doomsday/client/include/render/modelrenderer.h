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

#include <QList>
#include <QMap>

/**
 * The model renderer prepares available model assets for drawing (using ModelDrawable),
 * and keeps the set of needed ModelDrawable instances in memory.
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
     * Provides access to the common model-view-projection matrix used for rendering
     * models.
     *
     * @return Matrix uniform.
     */
    de::GLUniform &uMvpMatrix();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_MODELRENDERER_H
