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
class ModelDrawable : public AssetGroup
{
public:
    /// An error occurred during the loading of the model data. @ingroup errors
    DENG2_ERROR(LoadError);

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
     * Atlas to use for any textures needed by the model.
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

    void setAnimationTime(TimeDelta const &time);

    void draw() const;

    void drawInstanced(GLBuffer const &instanceAttribs) const;

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
