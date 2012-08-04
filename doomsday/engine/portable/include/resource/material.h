/**
 * @file material.h
 * Logical material. @ingroup resource
 *
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#include "p_maptypes.h"
#include "p_dmu.h"

#ifdef __cplusplus
extern "C" {
#endif

struct materialvariant_s;

/**
 * Initialize. Note that Material expects that initialization is done
 * but once during construction and that the owner will not attempt to
 * re-initialize later on.
 */
void Material_Initialize(material_t* mat);

void Material_Destroy(material_t* mat);

/**
 * Process a system tick event.
 */
void Material_Ticker(material_t* mat, timespan_t time);

/**
 * Add a new variant to the list of resources for this Material.
 * Material takes ownership of the variant.
 *
 * @param variant  Variant instance to add to the resource list.
 */
struct materialvariant_s* Material_AddVariant(material_t* mat,
    struct materialvariant_s* variant);

/**
 * Destroys all derived MaterialVariants linked with this Material.
 */
void Material_DestroyVariants(material_t* mat);

/**
 * Iterate over all derived MaterialVariants, making a callback for each.
 * Iteration ends once all variants have been visited, or immediately upon
 * a callback returning non-zero.
 *
 * @param callback  Callback to make for each processed variant.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Material_IterateVariants(material_t* mat,
    int (*callback)(struct materialvariant_s* variant, void* paramaters),
    void* paramaters);

/// @return  Definition associated with this.
struct ded_material_s* Material_Definition(const material_t* mat);

/**
 * Change the associated definition.
 * @param def  New definition. Can be @c NULL.
 */
void Material_SetDefinition(material_t* mat, struct ded_material_s* def);

/// Retrieve dimensions in logical world units.
const Size2* Material_Size(const material_t* mat);

/**
 * Change dimensions.
 * @param size  New dimensions in logical world units.
 */
void Material_SetSize(material_t* mat, const Size2Raw* size);

/// @return  Logical width.
int Material_Width(const material_t* mat);

void Material_SetWidth(material_t* mat, int width);

/// @return  Logical height.
int Material_Height(const material_t* mat);

void Material_SetHeight(material_t* mat, int height);

/// @return  @see materialFlags
short Material_Flags(const material_t* mat);

/**
 * Change the public Material Flags.
 * @param flags  @see materialFlags
 */
void Material_SetFlags(material_t* mat, short flags);

/// @return  @c true if Material is not derived from an original game resource.
boolean Material_IsCustom(const material_t* mat);

/// @return  @c true if Material belongs to one or more anim groups.
boolean Material_IsGroupAnimated(const material_t* mat);

/// @return  @c true if Material should be replaced with Sky.
boolean Material_IsSkyMasked(const material_t* mat);

/// @return  @c true if Material is considered drawable.
boolean Material_IsDrawable(const material_t* mat);

/// @return  @c true if one or more animation stages are defined as "glowing".
boolean Material_HasGlow(material_t* mat);

/// @return  @c true if there is an active translation.
boolean Material_HasTranslation(const material_t* mat);

/// @return  Number of layers.
int Material_LayerCount(const material_t* mat);

/// Change the group animation status.
void Material_SetGroupAnimated(material_t* mat, boolean yes);

/// @return  Prepared state of this material.
byte Material_Prepared(const material_t* mat);

/**
 * Change the prepared status of this material.
 * @param state  @c 0: Not yet prepared.
 *               @c 1: Prepared from original game textures.
 *               @c 2: Prepared from custom or replacement textures.
 */
void Material_SetPrepared(material_t* mat, byte state);

/// @return  Identifier of the primary MaterialBind associated with this (may return @c 0 - no binding).
materialid_t Material_PrimaryBind(const material_t* mat);

/**
 * Change the identifier of the primary binding associated with this.
 * @param bindId  New identifier.
 */
void Material_SetPrimaryBind(material_t* mat, materialid_t bindId);

/// @return  MaterialEnvironmentClass.
material_env_class_t Material_EnvironmentClass(const material_t* mat);

/**
 * Change the associated environment class.
 * \todo If attached to a Map Surface update accordingly!
 * @param envClass  New MaterialEnvironmentClass.
 */
void Material_SetEnvironmentClass(material_t* mat, material_env_class_t envClass);

/// @return  Detail Texture linked to this else @c NULL
struct texture_s* Material_DetailTexture(material_t* mat);

/**
 * Change the Detail Texture linked to this.
 * @param tex  Texture to be linked with.
 */
void Material_SetDetailTexture(material_t* mat, struct texture_s* tex);

/// @return  Detail Texture blending factor for this [0...1].
float Material_DetailStrength(material_t* mat);

/**
 * Change the Detail Texture strength factor for this.
 * @param strength  New strength value (will be clamped to [0...1]).
 */
void Material_SetDetailStrength(material_t* mat, float strength);

/// @return  Detail Texture scale factor for this [0...1].
float Material_DetailScale(material_t* mat);

/**
 * Change the Detail Texture scale factor for this.
 * @param scale  New scale value (will be clamped to [0...1]).
 */
void Material_SetDetailScale(material_t* mat, float scale);

/// @return  Shiny Texture linked to this else @c NULL
struct texture_s* Material_ShinyTexture(material_t* mat);

/**
 * Change the Shiny Texture linked to this.
 * @param tex  Texture to be linked with.
 */
void Material_SetShinyTexture(material_t* mat, struct texture_s* tex);

/// @return  Shiny Texture blendmode for this.
blendmode_t Material_ShinyBlendmode(material_t* mat);

/**
 * Change the Shiny Texture blendmode for this.
 * @param blendmode  New blendmode value.
 */
void Material_SetShinyBlendmode(material_t* mat, blendmode_t blendmode);

/// @return  Shiny Texture strength factor for this.
float Material_ShinyStrength(material_t* mat);

/**
 * Change the Shiny Texture strength factor for this.
 * @param strength  New strength value (will be clamped to [0...1]).
 */
void Material_SetShinyStrength(material_t* mat, float strength);

/// @return  Shiny Texture minColor (RGB triplet) for this.
const float* Material_ShinyMinColor(material_t* mat);

/**
 * Change the Shiny Texture minColor for this.
 * @param colorRGB  New RGB color values (each component will be clamped to [0...1]).
 */
void Material_SetShinyMinColor(material_t* mat, const float colorRGB[3]);

/// @return  ShinyMask Texture linked to this else @c NULL
struct texture_s* Material_ShinyMaskTexture(material_t* mat);

/**
 * Change the ShinyMask Texture linked to this.
 * @param tex  Texture to be linked with.
 */
void Material_SetShinyMaskTexture(material_t* mat, struct texture_s* tex);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_MATERIAL_H */
