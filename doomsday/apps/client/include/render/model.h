/** @file render/model.h  Drawable model.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/Timeline>
#include <de/MultiAtlas>

#include <QHash>
#include <QFlags>

namespace render {

/**
 * Drawable model with client-specific extra information, e.g., animation
 * sequences.
 *
 * @todo Refactor: This could look more like a proper class, yes? -jk
 */
struct Model : public de::ModelDrawable
{
    static de::String const DEF_TIMELINE;

//---------------------------------------------------------------------------------------

    de::String identifier;

    /**
     * Animation sequence definition.
     */
    struct AnimSequence
    {
        de::String name;        ///< Name of the sequence.
        de::Record const *def;  ///< Record describing the sequence (in asset metadata).
        de::Timeline *timeline = nullptr; ///< Script timeline (owned).
        de::String sharedTimeline; ///< Name of shared timeline (if specified).

        AnimSequence(de::String const &n, de::Record const &d);
    };

    struct StateAnims : public QMap<de::String, QList<AnimSequence>>
    {};

    enum Flag
    {
        AutoscaleToThingHeight = 0x1,
        ThingOpacityAsAmbientLightAlpha = 0x2,
        ThingFullBrightAsAmbientLight = 0x4,
        DefaultFlags = AutoscaleToThingHeight
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Alignment
    {
        NotAligned,
        AlignToView,
        AlignToMomentum,
        AlignRandomly
    };

//---------------------------------------------------------------------------------------

    ~Model();

//---------------------------------------------------------------------------------------

    std::unique_ptr<de::MultiAtlas::AllocGroup> textures;

    Flags     flags      = DefaultFlags;
    Alignment alignYaw   = NotAligned;
    Alignment alignPitch = NotAligned;
    float     pspriteFOV = 0.0f; // Custom override of the fixed psprite FOV.

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
    QHash<de::String, de::Timeline *> timelines;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Model::Flags)

} // namespace render

#endif // DENG_CLIENT_RENDER_MODEL_H

