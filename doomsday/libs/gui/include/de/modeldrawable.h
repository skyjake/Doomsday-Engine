/** @file modeldrawable.h  Drawable specialized for 3D models.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/animation.h>
#include <de/asset.h>
#include <de/atlastexture.h>
#include <de/bitarray.h>
#include <de/deletable.h>
#include <de/file.h>
#include <de/glprogram.h>
#include <de/glstate.h>
#include <de/vector.h>

#include <functional>

namespace de {

class GLBuffer;

/**
 * Drawable that is constructed out of a 3D model file and texture map images.
 *
 * 3D model data is loaded using the Open Asset Import Library that supports
 * multiple different source formats.
 *
 * Lifetime.
 *
 * @par Animation
 *
 * ModelDrawable supports skeletal animation sequences.
 *
 * The nested class ModelDrawable::Animator is responsible for keeping track of
 * a model's animation state. When drawing an animated model, in addition to
 * having a ModelDrawable instance, one needs to create a
 * ModelDrawable::Animator and pass it to the draw() method.
 *
 * @par Textures and materials
 *
 * The model is composed of one or more meshes. Each mesh has a set of texture
 * maps associated with it (e.g., diffuse, normals, etc.). A set of texture
 * maps for all meshes is called a "material". ModelDrawable may have multiple
 * materials, but only one of them is active for drawing at a time. Only one
 * copy of each texture image is stored in the atlas even though multiple
 * materials use it.
 *
 * Internally, each material corresponds to a separate static vertex buffer.
 * The vertex coordinates in each buffer are prepared with the textures'
 * locations on the atlas. The active material can thus be switched at any time
 * during drawing, e.g., between rendering passes, without incurring a
 * performance penalty.
 *
 * @todo Refactor: Split the non-Assimp specific parts into a MeshDrawable base
 * class, so it can be used with meshes generated procedurally (e.g., the map),
 * taking advantage of the rendering pass and instancing features.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC ModelDrawable : public AssetGroup
{
public:
    /// An error occurred during the loading of the model data. @ingroup errors
    DE_ERROR(LoadError);

    /// There was a shader program related problem. @ingroup errors
    DE_ERROR(ProgramError);

    enum TextureMap // note: enum values used as indices internally
    {
        Diffuse = 0,    ///< Surface color and opacity.
        Normals = 1,    /**< Normal map where RGB values are directly interpreted
                             as vectors. Blue 255 is Z+1 meaning straight up.
                             Color value 128 means zero. The default normal vector
                             pointing straight away from the surface is therefore
                             (128, 128, 255) => (0, 0, 1). */
        Specular = 2,   ///< Specular color (RGB) and reflection sharpness (A).
        Emissive = 3,   /**< Additional light emitted by the surface that is not
                             affected by external factors. */
        Height = 4,     /**< Height values are converted to a normal map. Lighter
                             regions are higher than dark regions. */

        Unknown
    };

    static TextureMap textToTextureMap(const String &text);
    static String     textureMapToText(TextureMap map);

    /**
     * Animation state for a model. There can be any number of ongoing animations,
     * targeting individual nodes of a model.
     *
     * @ingroup gl
     */
    class LIBGUI_PUBLIC Animator : public Deletable, public ISerializable
    {
    public:
        /**
         * Specialized animators may derive from OngoingSequence to extend the amount of
         * data associated with each running animation sequence.
         */
        class LIBGUI_PUBLIC OngoingSequence : public ISerializable
        {
        public:
            enum Flag { ClampToDuration = 0x1, Defaults = 0 };

        public:
            int     animId;   ///< Which animation to use in a ModelDrawable.
            ddouble time;     ///< Animation time.
            ddouble duration; ///< Animation duration.
            String  node;     ///< Target node.
            Flags   flags;

        public:
            /**
             * Called after the basic parameters of the animation have been set for
             * a newly constructed sequence.
             */
            virtual void initialize();

            DE_CAST_METHODS()

            /**
             * Determines if the sequence is at its duration or past it.
             */
            bool atEnd() const;

            // ISerializable.
            void operator>>(Writer &to) const override;
            void operator<<(Reader &from) override;

            /**
             * Constructs an OngoingSequence instance. This is used by default if no other
             * constructor is provided.
             *
             * @return  OngoingSequence instance, to be owned by ModelDrawable::Animator.
             */
            static OngoingSequence *make();
        };

        typedef std::function<OngoingSequence * ()> Constructor;

        /// Referenced node or animation was not found in the model. @ingroup errors
        DE_ERROR(InvalidError);

        enum Flag {
            /**
             * Node transformations always done, even when there are no
             * animation sequences.
             */
            AlwaysTransformNodes = 0x1,

            DefaultFlags = 0
        };

    public:
        Animator(Constructor sequenceConstructor = OngoingSequence::make);
        Animator(const ModelDrawable &model,
                 Constructor sequenceConstructor = OngoingSequence::make);

        void setModel(const ModelDrawable &model);

        void setFlags(const Flags &flags, FlagOp op = SetFlags);

        Flags flags() const;

        /**
         * Returns the model with which this animation is being used.
         */
        const ModelDrawable &model() const;

        /**
         * Returns the number of ongoing animations.
         */
        int count() const;

        inline bool isEmpty() const { return !count(); }

        const OngoingSequence &at(int index) const;

        OngoingSequence &at(int index);

        bool isRunning(const String &animName, const String &rootNode = "") const;
        bool isRunning(int animId, const String &rootNode = "") const;

        OngoingSequence *find(const String &rootNode = "") const;
        OngoingSequence *find(int animId, const String &rootNode = "") const;

        /**
         * Starts an animation sequence. A previous sequence running on this node will
         * be automatically stopped.
         *
         * @param animName  Animation sequence name.
         * @param rootNode  Animation root.
         *
         * @return OngoingSequence.
         */
        OngoingSequence &start(const String &animName, const String &rootNode = "");

        /**
         * Starts an animation sequence. A previous sequence running on this node will
         * be automatically stopped.
         *
         * @param animId    Animation sequence number.
         * @param rootNode  Animation root.
         *
         * @return OngoingSequence.
         */
        OngoingSequence &start(int animId, const String &rootNode = "");

        void stop(int index);

        void clear();

        /**
         * Advances the animation state. Progresses ongoing animations and possibly
         * triggers new ones.
         *
         * @param elapsed  Duration of elapsed time.
         */
        virtual void advanceTime(TimeSpan elapsed);

        /**
         * Returns the time to be used when drawing the model.
         *
         * @param index  Animation index.
         *
         * @return Time in the model's animation sequence.
         */
        virtual ddouble currentTime(int index) const;

        /**
         * Determines an additional rotation angle for a given node. This is
         * called for each node when setting up the bone transformations.
         *
         * @param nodeName  Node name.
         *
         * @return Rotation axis (xyz) and angle (w; degrees).
         */
        virtual Vec4f extraRotationForNode(const String &nodeName) const;

        // ISerializable.
        void operator>>(Writer &to) const override;
        void operator<<(Reader &from) override;

    private:
        DE_PRIVATE(d)
    };

    /**
     * Interface for image loaders that provide the content for texture images
     * when given a path. The default loader just checks if there is an image
     * file in the file system at the given path.
     */
    class LIBGUI_PUBLIC IImageLoader
    {
    public:
        virtual ~IImageLoader() = default;

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
        virtual Image loadImage(const String &path) = 0;
    };

    /**
     * Rendering pass. When no rendering passes are specified, all the meshes
     * of the model are rendered in one pass with regular alpha blending.
     *
     * @todo Use GLState instead of than having individual GL parameters?
     * The state must be set up to not touch viewport, clipping, etc., though.
     */
    struct LIBGUI_PUBLIC Pass {
        String          name;
        BitArray        meshes;            ///< One bit per model mesh.
        GLProgram *     program = nullptr; ///< Shading program.
        gfx::BlendFunc  blendFunc{gfx::SrcAlpha, gfx::OneMinusSrcAlpha};
        gfx::BlendOp    blendOp    = gfx::Add;
        bool            depthWrite = true;
        gfx::Comparison depthFunc  = gfx::Less;

        bool operator==(const Pass &other) const
        {
            return name == other.name; // Passes are uniquely identified by names.
        }
    };

    struct LIBGUI_PUBLIC Passes : public List<Pass>
    {
        /**
         * Finds the pass with a given name. Performance is O(n) (i.e., suitable
         * for non-repeated use). The lookup is done case-sensitively.
         *
         * @param name  Pass name.
         *
         * @return Index of the pass. If not found, returns -1.
         */
        int findName(const String &name) const;
    };

    enum ProgramBinding { AboutToBind, Unbound };
    typedef std::function<void (GLProgram &, ProgramBinding)> ProgramBindingFunc;

    enum PassState { PassBegun, PassEnded };
    typedef std::function<void (const Pass &, PassState)> RenderingPassFunc;

    /**
     * Per-instance appearance parameters.
     */
    struct LIBGUI_PUBLIC Appearance
    {
        enum Flag { DefaultFlags = 0 };

        Flags flags = DefaultFlags;

        /**
         * Rendering passes. If omitted, all meshes are drawn with normal
         * alpha blending.
         */
        const Passes *drawPasses = nullptr;

        /**
         * Specifies the material used for each rendering pass. Size of the
         * list must equal the number of passes. If it doesn't, the default
         * material (0) is used during drawing.
         *
         * Switching the material doesn't change the internal GL resources
         * of the model. It only determines which vertex buffer is selected
         * for drawing the pass.
         */
        List<duint> passMaterial;

        /**
         * Sets a mask that specifies which rendering passes are enabled. Each
         * bit in the array corresponds to an element in @a drawPasses. An
         * empty mask (size zero) means that all passes are enabled.
         */
        BitArray passMask;

        ProgramBindingFunc programCallback = ProgramBindingFunc();
        RenderingPassFunc passCallback = RenderingPassFunc();
    };

    /**
     * Identifies a mesh.
     */
    struct LIBGUI_PUBLIC MeshId
    {
        duint index;
        duint material;

        MeshId(duint index, duint material = 0 /*default material*/)
            : index(index), material(material) {}
    };

    typedef List<TextureMap> Mapping;

    // Audiences:
    DE_AUDIENCE(AboutToGLInit, void modelAboutToGLInit(ModelDrawable &))

public:
    ModelDrawable();

    /**
     * Sets the object responsible for loading texture images.
     *
     * By default, ModelDrawable uses a simple loader that tries to load image
     * files directly from the file system.
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
     * Loads a model from a file. This is a synchronous operation and may take
     * a while, but can be called in a background thread.
     *
     * After loading, you must call glInit() before drawing it. glInit() will be
     * called automatically if needed.
     *
     * @param file  Model file to load.
     */
    void load(const File &file);

    /**
     * Finds the id of an animation that has the name @a name. Note that
     * animation names are optional.
     *
     * @param name  Animation name.
     *
     * @return Animation id, or -1 if not found.
     */
    int animationIdForName(const String &name) const;

    String animationName(int id) const;

    int animationCount() const;

    int meshCount() const;

    /**
     * Returns the number of the mesh with the given name.
     *
     * @param name  Mesh name.
     *
     * @return Mesh id, or -1 if no mesh with that name exists.
     */
    int meshId(const String &name) const;

    String meshName(int id) const;

    /**
     * Locates a material specified in the model by its name.
     *
     * @param name  Name of the material.
     *
     * @return Material id.
     */
    int materialId(const String &name) const;

    bool nodeExists(const String &name) const;

    /**
     * Atlas to use for any textures needed by the model. This is needed for
     * glInit().
     *
     * @param atlas  Atlas for model textures.
     */
    void setAtlas(IAtlas &atlas);

    /**
     * Sets the atlas to use for a specific type of textures. THis is needed for glInit(). Unlike
     * setAtlas(atlas) that uses the same atlas for everything, this will associate a certain type
     * of texture with a specific atlas.
     *
     * @param textureMap  Texture map. This
     * @param atlas       Atlas for model textures of type @a textureMap.
     */
    void setAtlas(TextureMap textureMap, IAtlas &atlas);

    /**
     * Removes the model's atlases. All allocations this model has made from the atlas
     * are freed.
     */
    void unsetAtlas();

    IAtlas *atlas(TextureMap textureMap = Unknown) const;

    /**
     * Sets which textures are to be passed to the model shader via the GL buffer.
     *
     * By default, the model only has a diffuse map. The user of ModelDrawable
     * must specify the indices for the other texture maps depending on how the
     * shader expects to receive them.
     *
     * @param mapsToUse  Up to four map types. The map at index zero will be specified
     *                   as the first texture bounds (@c aBounds in the shader), index
     *                   one will become the second texture bounds (@c aBounds2), etc.
     */
    void setTextureMapping(const Mapping& mapsToUse);

    static Mapping diffuseNormalsSpecularEmission();

    duint addMaterial();

    void resetMaterials();

    /**
     * Sets the texture map that is used if no other map is provided.
     *
     * @param textureType  Type of the texture.
     * @param atlasId      Identifier in the atlas.
     */
    void setDefaultTexture(TextureMap textureType, const Id &atlasId);

    /**
     * Sets or changes one of the texture maps used by a mesh. This can be
     * used to override the maps set up automatically by glInit() (that gets
     * information from the model file).
     *
     * @param mesh        Which mesh to modify.
     * @param textureMap  Texture to set.
     * @param path        Path of the texture image.
     */
    void setTexturePath(const MeshId &mesh, TextureMap textureMap,
                        const String &path);

    /**
     * Prepares a loaded model for drawing by constructing all the required GL
     * objects.
     *
     * This method will be called automatically when needed, however you can
     * also call it manually at a suitable time. Only call this from the main
     * (UI) thread.
     */
    void glInit();

    /**
     * Releases all the GL resources of the model.
     */
    void glDeinit();

    /**
     * Sets the GL program used for shading the model. This program is used if the
     * rendering passes don't specify other shaders.
     *
     * @param program  GL program.
     */
    void setProgram(GLProgram *program);

    GLProgram *program() const;

    /**
     * Draws the model.
     *
     * @param appearance  Appearance parameters.
     * @param animation   Animation state.
     */
    void draw(const Appearance *appearance = nullptr,
              const Animator *animation = nullptr) const;

    void draw(const Animator *animation) const {
        draw(nullptr, animation);
    }

    void drawInstanced(const GLBuffer &instanceAttribs,
                       const Animator *animation = nullptr) const;

    /**
     * When a draw operation is ongoing, returns the current rendering pass.
     * Otherwise returns nullptr.
     */
    const Pass *currentPass() const;

    /**
     * When a draw operation is ongoing, returns the current GL program.
     * Otherwise returns nullptr.
     */
    GLProgram *currentProgram() const;

    /**
     * Dimensions of the default pose, in model space.
     */
    Vec3f dimensions() const;

    /**
     * Center of the default pose, in model space.
     */
    Vec3f midPoint() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

//LIBGUI_PUBLIC uint qHash(const de::ModelDrawable::Pass &pass);

#endif // LIBGUI_MODELDRAWABLE_H
