/** @file resource/model.h  3D model resource (MD2/DMD)
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_RESOURCE_FRAMEMODEL_H
#define DE_RESOURCE_FRAMEMODEL_H

#include <doomsday/filesys/filehandle.h>
#ifdef __CLIENT__
#  include "resource/clienttexture.h"
#endif
#include <de/error.h>
#include <de/string.h>
#include <de/bitarray.h>
#include <de/vector.h>

/// Unique identifier associated with each model.
typedef uint modelid_t;

/// Special value used to signify an invalid model id.
#define NOMODELID 0

/**
 * 3D model resource used in Doomsday 1.x. FrameModel uses frame-based animation
 * where each frame of the animation is represented by an entire set of vertex positions.
 *
 * @ingroup resource
 */
class FrameModel
{
public:
    /// Referenced frame is missing. @ingroup errors
    DE_ERROR(MissingFrameError);

    /// Referenced skin is missing. @ingroup errors
    DE_ERROR(MissingSkinError);

    /// Referenced detail level is missing. @ingroup errors
    DE_ERROR(MissingDetailLevelError);

    /**
     * Classification/processing flags.
     */
    enum Flag {
        NoTextureCompression = 0x1 ///< Do not compress skin textures.
    };

    /**
     * Animation key-frame.
     */
    struct Frame
    {
        FrameModel &model;
        struct Vertex {
            de::Vec3f pos;
            de::Vec3f norm;
        };
        typedef de::List<Vertex> VertexBuf;
        VertexBuf vertices;
        de::Vec3f min;
        de::Vec3f max;
        de::String name;

        Frame(FrameModel &model, const de::String &name = de::String())
            : model(model), name(name)
        {}

        void bounds(de::Vec3f &min, de::Vec3f &max) const;

        float horizontalRange(float *top, float *bottom) const;
    };
    typedef de::List<Frame *> Frames;

    /**
     * Texture => Skin assignment.
     */
    struct Skin
    {
        de::String name;
        res::Texture *texture; // Not owned.

        Skin(const de::String &name = de::String(), res::Texture *texture = 0)
            : name(name), texture(texture)
        {}
    };
    typedef de::List<Skin> Skins;

    /**
     * Prepared model geometry uses lists of primitives.
     */
    struct Primitive
    {
        struct Element
        {
            de::Vec2f texCoord;
            int index; ///< Index into the model's vertex mesh.
        };
        typedef de::List<Element> Elements;
        Elements elements;
        bool triFan; ///< @c true= triangle fan; otherwise triangle strip.
    };
    typedef de::List<Primitive> Primitives;

    /**
     * Level of detail information.
     *
     * Used with DMD models to reduce complexity of the drawn model geometry.
     */
    struct DetailLevel
    {
        FrameModel &model;
        int level;
        Primitives primitives;

        DetailLevel(FrameModel &model, int level)
            : model(model), level(level)
        {}

        /**
         * Returns @c true iff the specified vertex @a number is in use for this
         * detail level.
         */
        bool hasVertex(int number) const;
    };
    typedef de::List<DetailLevel *> DetailLevels;

public:
    /**
     * Construct a new 3D model.
     */
    FrameModel(de::Flags flags = 0);

    /**
     * Determines whether the specified @a file appears to be in a recognised
     * model format.
     */
    static bool recognise(res::FileHandle &file);

    /**
     * Attempt to load a new model resource from the specified @a file.
     *
     * @param file         Handle for the model file to load from.
     * @param aspectScale  Optionally apply y-aspect scaling.
     *
     * @return  The new FrameModel (if any). Ownership is given to the caller.
     */
    static FrameModel *loadFromFile(res::FileHandle &file, float aspectScale = 1);

    /**
     * Returns the unique identifier associated with the model.
     */
    uint modelId() const;

    /**
     * Change the unique identifier associated with the model.
     *
     * @param newId  New identifier to apply.
     */
    void setModelId(uint newId);

    /**
     * Returns a copy of the current model flags.
     */
    de::Flags flags() const;

    /**
     * Change the model's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(de::Flags flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Lookup a model animation frame by @a name.
     *
     * @return  Unique number of the found frame; otherwise @c -1 (not found).
     */
    int frameNumber(const de::String& name) const;

    /**
     * Convenient method of determining whether the specified model animation
     * frame @a number is valid (i.e., a frame is defined for it).
     */
    inline bool hasFrame(int number) const {
        return (number >= 0 && number < frameCount());
    }

    /**
     * Retrieve a model animation frame by it's unique frame @a number.
     */
    Frame &frame(int number) const;

    /**
     * Returns the total number of model animation frames.
     */
    inline int frameCount() const { return frames().count(); }

    /**
     * Provides access to the model animation frames, for efficient traversal.
     */
    const Frames &frames() const;

    /**
     * Clear all model animation frames.
     */
    void clearAllFrames();

    /**
     * Lookup a model skin by @a name.
     *
     * @return  Unique number of the found skin; otherwise @c -1 (not found).
     */
    int skinNumber(const de::String& name) const;

    /**
     * Convenient method of determining whether the specified model skin @a number
     * is valid (i.e., a skin is defined for it).
     */
    inline bool hasSkin(int number) const {
        return (number >= 0 && number < skinCount());
    }

    /**
     * Retrieve a model skin by it's unique @a number.
     */
    Skin &skin(int number) const;

    /**
     * Append a new skin with the given @a name to the model. If a skin already
     * exists with this name it will be returned instead.
     *
     * @return  Reference to the (possibly new) skin.
     */
    Skin &newSkin(de::String name);

    /**
     * Returns the total number of model skins.
     */
    inline int skinCount() const { return skins().count(); }

    /**
     * Provides access to the model skins, for efficient traversal.
     */
    const Skins &skins() const;

    /**
     * Clear all model skin assignments.
     */
    void clearAllSkins();

    /**
     * Convenient method of accessing the primitive list used for drawing the
     * model with the highest degree of geometric fidelity (i.e., detail level
     * zero).
     */
    const Primitives &primitives() const;

    /**
     * Returns the total number of vertices used at detail level zero.
     */
    int vertexCount() const;

    /**
     * Convenient method of determining whether the specified model detail
     * @a level is valid (i.e., detail information is defined for it).
     */
    inline bool hasLod(int level) const {
        return (level >= 0 && level < lodCount());
    }

    /**
     * Returns the total number of detail levels for the model.
     */
    inline int lodCount() const { return lods().count(); }

    /**
     * Retrieve model detail information by it's unique @a level number.
     */
    DetailLevel &lod(int level) const;

    /**
     * Provides readonly access to the level of detail information.
     */
    const DetailLevels &lods() const;

    /// @todo Refactor away.
    const de::BitArray &lodVertexUsage() const;

private:
    DE_PRIVATE(d)
};

typedef FrameModel::DetailLevel FrameModelLOD;
typedef FrameModel::Frame FrameModelFrame;
typedef FrameModel::Skin FrameModelSkin;

#endif // DE_RESOURCE_FRAMEMODEL_H
