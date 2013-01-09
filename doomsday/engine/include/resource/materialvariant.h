/** @file materialvariant.h Logical Material Variant.
 *
 * @author Copyright &copy; 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIALVARIANT_H
#define LIBDENG_RESOURCE_MATERIALVARIANT_H

/// Material (Usage) Context identifiers.
typedef enum {
    MC_UNKNOWN = -1,
    MATERIALCONTEXT_FIRST = 0,
    MC_UI = MATERIALCONTEXT_FIRST,
    MC_MAPSURFACE,
    MC_SPRITE,
    MC_MODELSKIN,
    MC_PSPRITE,
    MC_SKYSPHERE,
    MATERIALCONTEXT_LAST = MC_SKYSPHERE
} materialcontext_t;

#define MATERIALCONTEXT_COUNT (MATERIALCONTEXT_LAST + 1 - MATERIALCONTEXT_FIRST )

/// @c true= val can be interpreted as a valid material context identifier.
#define VALID_MATERIALCONTEXT(val) ((val) >= MATERIALCONTEXT_FIRST && (val) <= MATERIALCONTEXT_LAST)

#ifdef __cplusplus

#include "def_data.h"
#include "resource/materialsnapshot.h"
#include "resource/material.h"
#include "resource/texture.h"

struct texturevariantspecification_s;

namespace de {

/**
 * @ingroup resource
 */
struct MaterialVariantSpec
{
    /// Usage context identifier.
    materialcontext_t context;

    /// Specification for the primary texture.
    struct texturevariantspecification_s *primarySpec;

    /**
     * Construct a default MaterialVariantSpec instance.
     */
    MaterialVariantSpec() : context(MC_UNKNOWN), primarySpec(0)
    {}

    /**
     * Construct a MaterialVariantSpec instance by duplicating @a other.
     */
    MaterialVariantSpec(MaterialVariantSpec const &other)
        : context(other.context), primarySpec(other.primarySpec)
    {}

    /**
     * Determines whether specification @a other is equal to this specification.
     *
     * @param other  The other specification.
     * @return  @c true if specifications are equal; otherwise @c false.
     *
     * Same as operator ==
     */
    bool compare(MaterialVariantSpec const &other) const;

    /**
     * Determines whether specification @a other is equal to this specification.
     * @see compare()
     */
    bool operator == (MaterialVariantSpec const &other) const {
        return compare(other);
    }

    /**
     * Determines whether specification @a other is NOT equal to this specification.
     * @see compare()
     */
    bool operator != (MaterialVariantSpec const &other) const {
        return !(*this == other);
    }
};

/**
 * @ingroup resource
 */
class MaterialVariant
{
public:
    /// Maximum number of layers a material supports.
    static int const max_layers = DDMAX_MATERIAL_LAYERS;

    /// Current state of a material layer.
    struct LayerState
    {
        /// Animation stage else @c -1 => layer not in use.
        int stage;

        /// Remaining (sharp) tics in the current stage.
        short tics;

        /// Intermark from the current stage to the next.
        float inter;

        /// Interpolated origin of the texture in material-space.
        /// @todo Does not belong at this level.
        float texOrigin[2];

        /// Interpolated glow strength factor.
        /// @todo Does not belong at this level.
        float glowStrength;
    };

public:
    /// The requested layer does not exist. @ingroup errors
    DENG2_ERROR(InvalidLayerError);

public:
    MaterialVariant(material_t &generalCase, MaterialVariantSpec const &spec,
                    ded_material_t const &def);
    ~MaterialVariant();

    /**
     * Retrieve the general case for this variant. Allows for a variant
     * reference to be used in place of a material (implicit indirection).
     *
     * @see generalCase()
     */
    inline operator material_t &() const {
        return generalCase();
    }

    /// @return  Superior material from which the variant was derived.
    material_t &generalCase() const;

    /// @return  MaterialVariantSpec which the variant was specialised using.
    MaterialVariantSpec const &spec() const;

    /**
     * Returns @c true if animation of the variant is currently paused (e.g.,
     * the variant is for use with an in-game render context and the client has
     * paused the game).
     */
    bool isPaused() const;

    /**
     * Process a system tick event.
     * @param time  Length of the tick in seconds.
     */
    void ticker(timespan_t time);

    /**
     * Reset the staged animation point for this Material.
     */
    void resetAnim();

    /**
     * Return the current state of @a layerNum for the variant.
     */
    LayerState const &layer(int layerNum);

    /**
     * Attach MaterialSnapshot data. MaterialVariant is given ownership of @a materialSnapshot.
     * @return  Same as @a materialSnapshot for caller convenience.
     */
    MaterialSnapshot &attachSnapshot(MaterialSnapshot &snapshot);

    /**
     * Detach MaterialSnapshot data. Ownership is relinquished to the caller.
     */
    MaterialSnapshot *detachSnapshot();

    /// @return  MaterialSnapshot data associated with the variant; otherwise @c 0.
    MaterialSnapshot *snapshot() const;

    /// @return  Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame() const;

    /**
     * Change the frame when the snapshot was last prepared/updated.
     * @param frame  Frame to mark the snapshot with.
     */
    void setSnapshotPrepareFrame(int frame);

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
    /// @return  Translated 'next' (or target) MaterialVariant if set, else this.
    MaterialVariant *translationNext();

    /// @return  Translated 'current' MaterialVariant if set, else this.
    MaterialVariant *translationCurrent();

    /// @return  Translation position [0..1]
    float translationPoint();

    /**
     * Change the translation target for this variant.
     *
     * @param current  Translated 'current' MaterialVariant.
     * @param next  Translated 'next' (or target) MaterialVariant.
     */
    void setTranslation(MaterialVariant *current, MaterialVariant *next);

    /**
     * Change the translation point for this variant.
     * @param inter  Translation point.
     */
    void setTranslationPoint(float inter);
#endif

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif // __cplusplus

struct materialvariant_s; // The materialvariant instance (opaque).

#endif /* LIBDENG_RESOURCE_MATERIALVARIANT_H */
