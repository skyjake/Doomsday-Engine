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
 * Lifetime.
 *
 * Texture maps.
 *
 * Animation.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC ModelDrawable : public AssetGroup
{
public:
    /// An error occurred during the loading of the model data. @ingroup errors
    DENG2_ERROR(LoadError);

    DENG2_DEFINE_AUDIENCE2(AboutToGLInit, void modelAboutToGLInit(ModelDrawable &))

    enum TextureMap // note: used as indices internally
    {
        Diffuse = 0,    ///< Surface color and opacity.
        Normals = 1,    /**< Normal map where RGB values are directly interpreted as vectors.
                             Blue 255 is Z+1 meaning straight up. Color value 128 means zero.
                             The default normal vector pointing straight away from the
                             surface is therefore (128, 128, 255) => (0, 0, 1). */
        Specular = 2,   ///< Specular color (RGB) and reflection sharpness (A).
        Emission = 3,   /**< Additional light emitted by the surface that is not affected by
                             external factors. */
        Height = 4,     /**< Height values are converted to a normal map. Lighter regions
                             are higher than dark regions. */

        Unknown
    };

    static TextureMap textToTextureMap(String const &text);

    /**
     * Animation state for a model. There can be any number of ongoing animations,
     * targeting individual nodes of a model.
     *
     * @ingroup gl
     */
    class LIBGUI_PUBLIC Animator
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
        Animator();
        Animator(ModelDrawable const &model);
        virtual ~Animator() {}

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

        bool isRunning(String const &animName, String const &rootNode = "") const;
        bool isRunning(int animId, String const &rootNode = "") const;

        /**
         * Starts an animation sequence. A previous sequence running on this node will
         * be automatically stopped.
         *
         * @param animName  Animation sequence name.
         * @param rootNode  Animation root.
         *
         * @return Animation.
         */
        Animation &start(String const &animName, String const &rootNode = "");

        /**
         * Starts an animation sequence. A previous sequence running on this node will
         * be automatically stopped.
         *
         * @param animId    Animation sequence number.
         * @param rootNode  Animation root.
         *
         * @return Animation.
         */
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

        /**
         * Returns the time to be used when drawing the model.
         *
         * @param index  Animation index.
         *
         * @return Time in the model's animation sequence.
         */
        virtual ddouble currentTime(int index) const;

    private:
        DENG2_PRIVATE(d)
    };

    /**
     * Interface for image loaders that provide the content for texture images when
     * given a path. The default loader just checks if there is an image file in the
     * file system at the given path.
     */
    class LIBGUI_PUBLIC IImageLoader
    {
    public:
        virtual ~IImageLoader() {}

        /**
         * Loads an image. If the image can't be loaded, the loader must throw an
         * exception explaining the reason for the failure.
         *
         * @param path  Path of the image. This is an absolute de::FS path inferred from
         *              the location of the source model file and the material metadata
         *              it contains.
         *
         * @return Loaded image.
         */
        virtual Image loadImage(String const &path) = 0;
    };

public:
    ModelDrawable();

    /**
     * Sets the object responsible for loading texture images.
     *
     * By default, ModelDrawable uses a simple loader that tries to load image files
     * directly from the file system.
     *
     * @param loader  Image loader.
     */
    void setImageLoader(IImageLoader &loader);

    void useDefaultImageLoader();

    /**
     * Releases all the data: the loaded model and any GL resources.
     */
    void clear();

    /**
     * Loads a model from a file. This is a synchronous operation and may take a while,
     * but can be called in a background thread.
     *
     * After loading, you must call glInit() before drawing it. glInit() will be
     * called automatically if needed.
     *
     * @param file  Model file to load.
     */
    void load(File const &file);

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

    /**
     * Locates a material specified in the model by its name.
     *
     * @param name  Name of the material
     *
     * @return Material id.
     */
    int materialId(String const &name) const;

    bool nodeExists(String const &name) const;

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

    typedef QList<TextureMap> Mapping;

    /**
     * Sets which textures are to be passed to the model shader via the GL buffer.
     *
     * By default, the model only has a diffuse map. The user of ModelDrawable must
     * specify the indices for the other texture maps depending on how the shader expects
     * to receive them.
     *
     * @param mapsToUse  Up to four map types. The map at index zero will be specified
     *                   as the first texture bounds (@c aBounds in the shader), index
     *                   one will become the second texture bounds (@c aBounds2), etc.
     */
    void setTextureMapping(Mapping mapsToUse);

    static Mapping diffuseNormalsSpecularEmission();

    /**
     * Sets the texture map that is used if no other map is provided.
     *
     * @param textureType  Type of the texture.
     * @param atlasId      Identifier in the atlas.
     */
    void setDefaultTexture(TextureMap textureType, Id const &atlasId);

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
     * Sets or changes one of the texture maps used by the model. This can be used to
     * override the maps set up automatically by glInit().
     *
     * @param materialId  Which material to modify.
     * @param textureMap  Texture to set.
     * @param path        Path of the texture image.
     */
    void setTexturePath(int materialId, TextureMap textureMap, String const &path);

    /**
     * Sets the GL program used for shading the model.
     *
     * @param program  GL program.
     */
    void setProgram(GLProgram &program);

    void unsetProgram();

    void draw(Animator const *animation = 0) const;

    void drawInstanced(GLBuffer const &instanceAttribs,
                       Animator const *animation = 0) const;

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
