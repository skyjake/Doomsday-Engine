/** @file modeldrawable.h  Drawable specialized for 3D models.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBGUI_MODELDRAWABLE_H
#define LIBGUI_MODELDRAWABLE_H

#include <de/Asset>
#include <de/File>
#include <de/GLProgram>
#include <de/AtlasTexture>
#include <de/Vector>

#include <QVariant>

namespace de {

class GLBuffer;

/**
 * Drawable that is constructed out of a 3D model.
 *
 * 3D model data is loaded using the Open Asset Import Library from multiple different
 * source formats.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC ModelDrawable : public AssetGroup
{
public:
    /// An error occurred during the loading of the model data. @ingroup errors
    DENG2_ERROR(LoadError);

    /**
     * Animation state for a model. There can be any number of ongoing animations,
     * targeting individual nodes of a model.
     *
     * @ingroup gl
     */
    class LIBGUI_PUBLIC AnimationState
    {
    public:
        struct Animation {
            int animId;         ///< Which animation to use.
            ddouble time;       ///< Animation time.
            String node;        ///< Target node.
            QVariant data;      ///< Additional data for derived classes.
        };

        /// Referenced node or animation was not found in the model. @ingroup errors
        DENG2_ERROR(InvalidError);

    public:
        AnimationState();
        AnimationState(ModelDrawable const &model);

        void setModel(ModelDrawable const &model);

        /**
         * Returns the model with which this animation is being used.
         */
        ModelDrawable const &model() const;

        /**
         * Returns the number of ongoing animations.
         */
        int count() const;

        inline bool isEmpty() const { return !count(); }

        Animation const &at(int index) const;

        Animation &at(int index);

        Animation &start(String const &animName, String const &rootNode = "");

        Animation &start(int animId, String const &rootNode = "");

        void stop(int index);

        void clear();

        /**
         * Advances the animation state. Progresses ongoing animations and possibly
         * triggers new ones.
         *
         * @param elapsed  Duration of elapsed time.
         */
        virtual void advanceTime(TimeDelta const &elapsed);

    private:
        DENG2_PRIVATE(d)
    };

public:
    ModelDrawable();

    /**
     * Loads a model from a file. This is a synchronous operation and may take a while,
     * but can be called in a background thread.
     *
     * After loading, you must call glInit() before drawing it. glInit() will be
     * called automatically if needed.
     *
     * @param path
     */
    void load(File const &file);

    /**
     * Releases all the data: the loaded model and any GL resources.
     */
    void clear();

    /**
     * Finds the id of an animation that has the name @a name. Note that animation
     * names are optional.
     *
     * @param name  Animation name.
     *
     * @return Animation id, or -1 if not found.
     */
    int animationIdForName(String const &name) const;

    int animationCount() const;

    bool nodeExists(String const &name) const;

    /**
     * Prepares a loaded model for drawing by constructing all the required GL objects.
     *
     * This method will be called automatically when needed, however you can also call it
     * manually at a suitable time. Only call this from the main (UI) thread.
     */
    void glInit();

    /**
     * Releases all the GL resources of the model.
     */
    void glDeinit();

    /**
     * Atlas to use for any textures needed by the model. This is needed for glInit().
     *
     * @param atlas  Atlas for model textures.
     */
    void setAtlas(AtlasTexture &atlas);

    /**
     * Removes the model's atlas. All allocations this model has made from the atlas
     * are freed.
     */
    void unsetAtlas();

    /**
     * Sets the GL program used for shading the model.
     *
     * @param program  GL program.
     */
    void setProgram(GLProgram &program);

    void unsetProgram();

    void draw(AnimationState const *animation = 0) const;

    void drawInstanced(GLBuffer const &instanceAttribs,
                       AnimationState const *animation = 0) const;

    /**
     * Dimensions of the default pose, in model space.
     */
    Vector3f dimensions() const;

    /**
     * Center of the default pose, in model space.
     */
    Vector3f midPoint() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_MODELDRAWABLE_H
