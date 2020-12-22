/** @file material.h  World material.
 *
 * @authors Copyright © 2009-2016 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#pragma once

#include "../res/texture.h"
#include "../audio/s_environ.h"
#include "mapelement.h"

#include <de/list.h>
#include <de/error.h>
#include <de/observers.h>
#include <de/vector.h>
#include <functional>

namespace world {

using namespace de;

class MaterialManifest;

/**
 * Logical material resource.
 *
 * @par Dimensions
 * Material dimensions are interpreted relative to the coordinate space in which the material
 * is used. For example, the dimensions of a Material in the map-surface usage context are
 * thought to be in "map/world space" units.
 *
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC Material : public MapElement
{
public:
    /// Notified when the material is about to be deleted.
    DE_AUDIENCE(Deletion,         void materialBeingDeleted(const Material &))

    /// Notified whenever the logical dimensions change.
    DE_AUDIENCE(DimensionsChange, void materialDimensionsChanged(Material &))

public:
    /**
     * Construct a new Material and attribute it with the given resource @a manifest.
     */
    Material(MaterialManifest &manifest);

    ~Material();

    /**
     * Returns the attributed MaterialManifest for the material.
     */
    MaterialManifest &manifest() const;

    /**
     * Returns the dimension metrics of the material.
     */
    const Vec2ui &dimensions() const;

    inline int width () const { return int(dimensions().x); }
    inline int height() const { return int(dimensions().y); }

    /**
     * Change the world dimensions of the material to @a newDimensions.
     */
    void setDimensions(const Vec2ui &newDimensions);

    void setWidth (int newWidth);
    void setHeight(int newHeight);

    /**
     * Returns @c true if the material is marked @em drawable.
     */
    inline bool isDrawable() const {
        return (_flags & DontDraw) == 0;
    }

    /**
     * Returns @c true if the material is marked @em sky-masked.
     */
    inline bool isSkyMasked() const {
        return (_flags & SkyMasked) != 0;
    }

    /**
     * Returns @c true if the material is marked @em valid.
     *
     * Materials are invalidated only when dependent resources (such as the definition
     * from which it was produced) are destroyed as a result of runtime file unloading.
     *
     * These 'orphaned' materials cannot be immediately destroyed as the game may be
     * holding on to pointers (which are considered eternal). Therefore, materials are
     * invalidated (disabled) and will be ignored until they can actually be destroyed
     * (e.g., the current game is reset or changed).
     */
    inline bool isValid() const {
        return (_flags & Valid) != 0;
    }

    virtual bool isAnimated() const;

    /**
     * Returns the attributed audio environment identifier for the material.
     */
    AudioEnvironmentId audioEnvironment() const;

    /**
     * Change the attributed audio environment for the material to @a newEnvironment.
     */
    void setAudioEnvironment(AudioEnvironmentId newEnvironment);

    /**
     * Change the do-not-draw property of the material according to @a yes.
     */
    void markDontDraw (bool yes = true);

    /**
     * Change the sky-masked property of the material according to @a yes.
     */
    void markSkyMasked(bool yes = true);

    /**
     * Change the is-valid property of the material according to @a yes.
     */
    void markValid    (bool yes = true);

    /**
     * Returns a human-friendly, textual name for the object.
     */
    virtual String describe() const;

    /**
     * Returns a human-friendly, textual description of the full material configuration.
     */
    virtual String description() const;

//- Layers ------------------------------------------------------------------------------

    /// The referenced layer does not exist. @ingroup errors
    //DE_ERROR(MissingLayerError);

    /**
     * Base class for modelling a logical layer.
     *
     * A layer in this context is a formalized extension mechanism for customizing the
     * visual composition of a material. Layers are primarily intended for the modelling
     * of animated texture layers.
     *
     * Each material is composed from one or more layers. Layers are arranged in a stack,
     * according to the order in which they should be drawn, from the bottom-most to
     * the top-most layer.
     */
    class LIBDOOMSDAY_PUBLIC Layer
    {
    public:
        /// The referenced stage does not exist. @ingroup errors
        DE_ERROR(MissingStageError);

        /**
         * Base class for a logical layer animation stage.
         */
        struct Stage
        {
            int tics;
            float variance;  ///< Stage variance (time).

            Stage(int tics, float variance) : tics(tics), variance(variance) {}
            Stage(const Stage &other) : tics(other.tics), variance(other.variance) {}
            virtual ~Stage() {}

            DE_CAST_METHODS()

            /**
             * Returns a human-friendly, textual description of the animation stage
             * configuration.
             */
            virtual String description() const = 0;
        };

    public:
        virtual ~Layer();

        DE_CAST_METHODS()

        /**
         * Returns a human-friendly, textual name for the type of material layer.
         */
        virtual String describe() const;

        /**
         * Returns a human-friendly, textual synopsis of the material layer.
         */
        String description() const;

        /**
         * Returns the total number of animation stages for the material layer.
         */
        inline int stageCount() const { return _stages.sizei(); }

        /**
         * Returns @c true if the material layer is animated; otherwise @c false.
         */
        inline bool isAnimated() const { return stageCount() > 1; }

        /**
         * Lookup a material layer animation Stage by it's unique @a index.
         *
         * @param index  Index of the stage to lookup. Will be cycled into valid range.
         */
        Stage &stage(int index) const;

        int nextStageIndex(int index) const;

    protected:
        typedef List<Stage *> Stages;
        Stages _stages;
    };

    /**
     * Returns the number of material layers.
     */
    inline int layerCount() const { return _layers.sizei(); }

    /**
     * Add a new layer at the given layer stack position.
     *
     * @note As this alters the layer state, any existing client side MaterialAnimators
     * will need to be reconfigured/destroyed as they will no longer be valid.
     *
     * @param layer  Layer to add. Material takes ownership.
     * @param index  Numeric position in the layer stack at which to add the layer.
     */
    void addLayerAt(Layer *layer, int index);

    bool hasAnimatedTextureLayers() const;

    /**
     * Lookup a Layer by it's unique @a index.
     */
    inline Layer &layer(int index) const {
        DE_ASSERT(index >= 0 && index < layerCount());
        return *_layers[index];
    }

    inline Layer *layerPtr(int index) const {
        if (index >= 0 && index < layerCount()) return _layers[index];
        return nullptr;
    }

    /**
     * Destroys all the material's layers.
     *
     * @note As this alters the layer state, any existing client side MaterialAnimators
     * will need to be reconfigured/destroyed as they will no longer be valid.
     */
    void clearAllLayers();

protected:
    int property(DmuArgs &args) const;

public:
    /// Register the console commands and variables of this module.
    static void consoleRegister();

    enum Flag
    {
        //Unused1      = MATF_UNUSED1,
        DontDraw     = MATF_NO_DRAW,  ///< Map surfaces using the material should never be drawn.
        SkyMasked    = MATF_SKYMASK,  ///< Apply sky masking for map surfaces using the material.

        Valid        = 0x8,           ///< Marked as @em valid.
        DefaultFlags = Valid
    };

private:
    /// Layers (owned), from bottom-most to top-most draw order.
    List<Layer *> _layers; // Heavily used; visible for inline access.
    Flags _flags = DefaultFlags;

    DE_PRIVATE(d)
};

typedef Material::Layer MaterialLayer;

} // namespace world
