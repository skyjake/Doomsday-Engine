/** @file render/model.h  Drawable model.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_RENDER_MODEL_H
#define DENG_CLIENT_RENDER_MODEL_H

#include <de/Record>
#include <de/ModelDrawable>
#include <de/Scheduler>
#include <de/MultiAtlas>

#include <QHash>

namespace render {

/**
 * Drawable model with client-specific extra information, e.g., animation
 * sequences.
 *
 * @todo Refactor: This could look more like a proper class, yes? -jk
 */
struct Model : public de::ModelDrawable
{
    /**
     * Animation sequence definition.
     */
    struct AnimSequence
    {
        de::String name;        ///< Name of the sequence.
        de::Record const *def;  ///< Record describing the sequence (in asset metadata).
        de::Scheduler *timeline = nullptr; ///< Script timeline (owned).
        de::String sharedTimeline; ///< Name of shared timeline (if specified).

        AnimSequence(de::String const &n, de::Record const &d);
    };
    typedef QList<AnimSequence> AnimSequences;

    struct StateAnims : public QMap<de::String, AnimSequences> {};

    std::unique_ptr<de::MultiAtlas::AllocGroup> textures;

    bool autoscaleToThingHeight = true;
    bool alignToViewYaw         = false;
    bool alignToViewPitch       = false;

    /// Combined scaling and rotation of the model.
    de::Matrix4f transformation;

    de::Vector3f offset;

    de::gl::Cull cull = de::gl::Back;

    QHash<de::String, de::duint> materialIndexForName;

    /// Rendering passes. Will not change after init.
    Passes passes;

    /// Animation sequences.
    StateAnims animations;

    /// Shared timelines (not sequence-specific). Owned.
    QHash<de::String, de::Scheduler *> timelines;

    ~Model();
};

} // namespace render

#endif // DENG_CLIENT_RENDER_MODEL_H

