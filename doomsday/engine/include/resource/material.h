/** @file material.h Logical material.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIAL_H
#define LIBDENG_RESOURCE_MATERIAL_H

#include "map/p_mapdata.h"
#include "map/p_dmu.h"

struct ded_material_s;
struct materialvariant_s;

typedef enum {
    MEC_UNKNOWN = -1,
    MEC_FIRST = 0,
    MEC_METAL = MEC_FIRST,
    MEC_ROCK,
    MEC_WOOD,
    MEC_CLOTH,
    NUM_MATERIAL_ENV_CLASSES
} material_env_class_t;

#define VALID_MATERIAL_ENV_CLASS(v) ((v) >= MEC_FIRST && (v) < NUM_MATERIAL_ENV_CLASSES)

/**
 * Prepared states:
 * - 0 Not yet prepared.
 * - 1 Prepared from original game textures.
 * - 2 Prepared from custom or replacement textures.
 *
 * @ingroup resource
 */
struct material_s;
typedef struct material_s material_t;

#ifdef __cplusplus

#include <QList>
#include <de/Error>
#include "uri.hh"

namespace de {

class MaterialAnim;
class MaterialManifest;
class MaterialVariant;
struct MaterialVariantSpec;

/**
 * @ingroup resource
 */
class Material
{
public:
    /// A list of variant instances.
    typedef QList<MaterialVariant *> Variants;

    struct Decoration
    {
        ded_decorlight_t *def;
        Decoration(ded_decorlight_t &_def) : def(&_def)
        {}
    };

    /// A list of decorations.
    typedef QList<Material::Decoration *> Decorations;

public:
    /// The material is not a member of an animation group. @ingroup errors
    DENG2_ERROR(NoAnimGroupError);
};

} // namespace de

extern "C" {
#endif

/**
 * Construct a new material.
 *
 * @param flags  @see materialFlags
 * @param def  Definition for the material, if any.
 * @param dimensions  Dimensions of the material in map coordinate space units.
 * @param envClass  Environment class for the material.
 */
material_t *Material_New(short flags, ded_material_t *def, Size2Raw *dimensions,
                         material_env_class_t envClass);

void Material_Delete(material_t *mat);

void Material_AddDecoration(material_t *mat, ded_decorlight_t *def);

int Material_DecorationCount(material_t const *mat);

/**
 * Process a system tick event.
 */
void Material_Ticker(material_t *mat, timespan_t time);

int Material_VariantCount(material_t const *mat);

/**
 * Destroys all derived MaterialVariants linked with this Material.
 */
void Material_ClearVariants(material_t *mat);

/// @return  Definition associated with this.
struct ded_material_s *Material_Definition(material_t const *mat);

/**
 * Change the associated definition.
 * @param def  New definition. Can be @c NULL.
 */
void Material_SetDefinition(material_t *mat, struct ded_material_s *def);

/// Returns the dimensions of the material in map coordinate space units.
Size2 const *Material_Dimensions(material_t const *mat);

/**
 * Change the world dimensions of the material.
 * @param newDimensions  New dimensions in map coordinate space units.
 */
void Material_SetDimensions(material_t *mat, Size2Raw const *size);

/// @return  Width of the material in map coordinate space units.
int Material_Width(material_t const *mat);

void Material_SetWidth(material_t *mat, int width);

/// @return  Height of the material in map coordinate space units.
int Material_Height(material_t const *mat);

void Material_SetHeight(material_t *mat, int height);

/// @return  @see materialFlags
short Material_Flags(material_t const *mat);

/**
 * Change the public Material Flags.
 * @param flags  @ref materialFlags
 */
void Material_SetFlags(material_t *mat, short flags);

/**
 * Returns @c true if the material is considered to be @em valid. A material
 * can only be invalidated when resources it depends on (such as the definition
 * from which it was produced) are removed as result of runtime file unloading.
 *
 * We can't yet purge these 'orphaned' materials as the game may be holding on
 * to pointers (which are considered eternal). Instead, an invalid material is
 * ignored until such time as the current game is reset or is changed.
 */
boolean Material_IsValid(material_t const *mat);

/// @return  @c true= the material is animated.
boolean Material_IsAnimated(material_t const *mat);

/// Returns @c true if the material is considered @em skymasked.
boolean Material_IsSkyMasked(material_t const *mat);

/// Returns @c true if the material is considered drawable.
boolean Material_IsDrawable(material_t const *mat);

/// Returns @c true if one or more (light) decorations are defined for the material.
boolean Material_HasDecorations(material_t const *mat);

/// Returns @c true if one or more of the material's layers are glowing.
boolean Material_HasGlow(material_t const *mat);

/// @return  Number of layers.
int Material_LayerCount(material_t const *mat);

/// @return  Prepared state of this material.
byte Material_Prepared(material_t const *mat);

/**
 * Change the prepared status of this material.
 * @param state  @c 0: Not yet prepared.
 *               @c 1: Prepared from original game textures.
 *               @c 2: Prepared from custom or replacement textures.
 */
void Material_SetPrepared(material_t *mat, byte state);

/// @return  Unique identifier of the MaterialManifest for the material.
materialid_t Material_ManifestId(material_t const *mat);

/**
 * Change the unique identifier of the MaterialManifest for the material.
 * @param id  New identifier.
 *
 * @todo Refactor away.
 */
void Material_SetManifestId(material_t *mat, materialid_t id);

/// @return  MaterialEnvironmentClass.
material_env_class_t Material_EnvironmentClass(material_t const *mat);

/**
 * Change the associated environment class.
 * @todo If attached to a Map Surface update accordingly!
 * @param envClass  New MaterialEnvironmentClass.
 */
void Material_SetEnvironmentClass(material_t *mat, material_env_class_t envClass);

/// @return  Detail Texture linked to this else @c NULL
struct texture_s *Material_DetailTexture(material_t *mat);

/**
 * Change the Detail Texture linked to this.
 * @param tex  Texture to be linked with.
 */
void Material_SetDetailTexture(material_t *mat, struct texture_s *tex);

/// @return  Detail Texture blending factor for this [0..1].
float Material_DetailStrength(material_t *mat);

/**
 * Change the Detail Texture strength factor for this.
 * @param strength  New strength value (will be clamped to [0..1]).
 */
void Material_SetDetailStrength(material_t *mat, float strength);

/// @return  Detail Texture scale factor for this [0..1].
float Material_DetailScale(material_t *mat);

/**
 * Change the Detail Texture scale factor for this.
 * @param scale  New scale value (will be clamped to [0..1]).
 */
void Material_SetDetailScale(material_t *mat, float scale);

/// @return  Shiny Texture linked to this else @c NULL
struct texture_s *Material_ShinyTexture(material_t *mat);

/**
 * Change the Shiny Texture linked to this.
 * @param tex  Texture to be linked with.
 */
void Material_SetShinyTexture(material_t *mat, struct texture_s *tex);

/// @return  Shiny Texture blendmode for this.
blendmode_t Material_ShinyBlendmode(material_t *mat);

/**
 * Change the Shiny Texture blendmode for this.
 * @param blendmode  New blendmode value.
 */
void Material_SetShinyBlendmode(material_t *mat, blendmode_t blendmode);

/// @return  Shiny Texture strength factor for this.
float Material_ShinyStrength(material_t *mat);

/**
 * Change the Shiny Texture strength factor for this.
 * @param strength  New strength value (will be clamped to [0..1]).
 */
void Material_SetShinyStrength(material_t *mat, float strength);

/// @return  Shiny Texture minColor (RGB triplet) for this.
float const *Material_ShinyMinColor(material_t *mat);

/**
 * Change the Shiny Texture minColor for this.
 * @param colorRGB  New RGB color values (each component will be clamped to [0..1]).
 */
void Material_SetShinyMinColor(material_t *mat, float const colorRGB[3]);

/// @return  ShinyMask Texture linked to this else @c NULL
struct texture_s *Material_ShinyMaskTexture(material_t *mat);

/**
 * Change the ShinyMask Texture linked to this.
 * @param tex  Texture to be linked with.
 */
void Material_SetShinyMaskTexture(material_t *mat, struct texture_s *tex);

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
/// @return  @c true= the material belongs to one or more anim groups.
boolean Material_IsGroupAnimated(material_t const *mat);

/// @return  @c true if there is an active translation.
boolean Material_HasTranslation(material_t const *mat);

/// Change the group animation status.
void Material_SetGroupAnimated(material_t *mat, boolean yes);
#endif

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param material  Material instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Material_GetProperty(material_t const *material, setargs_t *args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param material  Material instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Material_SetProperty(material_t *material, setargs_t const *args);

#ifdef __cplusplus
} // extern "C"

de::MaterialManifest &Material_Manifest(material_t const *material);

/**
 * Provides access to the list of decorations for efficient traversal.
 */
de::Material::Decorations const &Material_Decorations(material_t const *mat);

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
/**
 * Returns the animation group for the material.
 */
de::MaterialAnim &Material_AnimGroup(material_t *mat);
#endif

/**
 * Choose/create a variant of the material which fulfills @a spec.
 *
 * @param material      Material instance.
 * @param spec          Specification for the derivation of @a material.
 * @param smooth        @c true= Select the current frame if the material is group-animated.
 * @param canCreate     @c true= Create a new variant if no suitable one exists.
 *
 * @return  Chosen variant; otherwise @c NULL if none suitable and not creating.
 */
de::MaterialVariant *Material_ChooseVariant(material_t *mat,
    de::MaterialVariantSpec const &spec, bool smoothed = true, bool canCreate = false);

/**
 * Provides access to the list of variant instances for efficient traversal.
 */
de::Material::Variants const &Material_Variants(material_t const *mat);

#endif

#ifdef LIBDENG_OLD_MATERIAL_ANIM_METHOD
#ifdef __cplusplus

#include <QList>
#include <de/Error>

namespace de {

/**
 * @ingroup resource
 */
class MaterialAnim
{
public:
    /**
     * One frame in the animation.
     */
    class Frame
    {
    public:
        Frame(material_t &mat, ushort _tics, ushort _randomTics)
            : material_(&mat), tics_(_tics), randomTics_(_randomTics)
        {}

        /**
         * Returns the material of the frame.
         */
        material_t &material() const {
            return *material_;
        }

        /**
         * Returns the duration of the frame in (sharp) tics.
         */
        ushort tics() const {
            return tics_;
        }

        /**
         * Returns the random part of the frame duration in (sharp) tics.
         */
        ushort randomTics() const {
            return randomTics_;
        }

    private:
        material_t *material_;
        ushort tics_;
        ushort randomTics_;
    };

    /// All frames in the animation.
    typedef QList<Frame> Frames;

public:
    /// An invalid frame reference was specified. @ingroup errors
    DENG2_ERROR(InvalidFrameError);

public:
    MaterialAnim(int id, int flags);

    /**
     * Progress the animation one frame forward.
     */
    void animate();

    /**
     * Restart the animation over from the first frame.
     */
    void reset();

    /**
     * Returns the animation's unique identifier.
     */
    int id() const;

    /**
     * Returns the animation's @ref animationGroupFlags.
     */
    int flags() const;

    /**
     * Returns the total number of frames in the animation.
     */
    int frameCount() const;

    /**
     * Lookup a frame in the animation by number.
     *
     * @param number  Frame number to lookup.
     * @return  Found animation frame.
     */
    Frame &frame(int number);

    /**
     * Extend the animation by adding a new frame to the end of the sequence.
     *
     * @param mat  Material for the frame.
     * @param tics  Duration of the frame in (sharp) tics.
     * @param randomTics  Random part of the frame duration in (sharp) tics.
     */
    void addFrame(material_t &mat, int tics, int randomTics);

    /**
     * Returns @c true iff @a mat is used by one or more frames in the animation.
     *
     * @param mat  Material to search for.
     */
    bool hasFrameForMaterial(material_t const &mat) const;

    /**
     * Provides access to the frame list for efficient traversal.
     */
    Frames const &allFrames() const;

private:
    /// Unique identifier.
    int id_;

    /// @ref animationGroupFlags.
    int flags_;

    /// Current frame index.
    int index;

    int maxTimer;
    int timer;

    /// All animation frames.
    Frames frames;
};

} // namespace de
#endif // __cplusplus
#endif // LIBDENG_OLD_MATERIAL_ANIM_METHOD

#endif /* LIBDENG_RESOURCE_MATERIAL_H */
