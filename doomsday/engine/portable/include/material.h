/**\file material.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_MATERIAL_H
#define LIBDENG_MATERIAL_H

#include "p_maptypes.h"
#include "p_dmu.h"

struct materialvariant_s;

/**
 * Initialize. Note that Material expects that initialization is done
 * but once during construction and that the owner will not attempt to
 * re-initialize later on.
 */
void Material_Initialize(material_t* mat);

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

/// @return  Definition from which this Material was derived,
///          else @c NULL if generated automatically.
struct ded_material_s* Material_Definition(const material_t* mat);

/// Retrieve logical dimensions.
void Material_Dimensions(const material_t* mat, int* width, int* height);

/// @return  Logical width.
int Material_Width(const material_t* mat);

/// @return  Logical height.
int Material_Height(const material_t* mat);

/// @return  @see materialFlags
short Material_Flags(const material_t* mat);

/// @return  @c true if Material is not derived from an original game resource.
boolean Material_IsCustom(const material_t* mat);

/// @return  @c true if Material belongs to one or more anim groups.
boolean Material_IsGroupAnimated(const material_t* mat);

/// @return  @c true if Material should be replaced with Sky.
boolean Material_IsSkyMasked(const material_t* mat);

/// @return  @c true if Material should be rendered.
boolean Material_IsDrawable(const material_t* mat);

/// @return  Number of layers defined by this Material.
int Material_LayerCount(const material_t* mat);

/**
 * Changed the group animation status of this Material.
 */
void Material_SetGroupAnimated(material_t* mat, boolean yes);

/// @return  Unique MaterialBind identifier.
uint Material_BindId(const material_t* mat);

/**
 * Set the MaterialBind identifier for this Material.
 *
 * @param bindId  New identifier.
 */
void Material_SetBindId(material_t* mat, uint bindId);

/// @return  Environmental sound class.
material_env_class_t Material_EnvClass(const material_t* mat);

/**
 * Change the environmental sound class for this Material.
 * \todo If attached to a Map Surface update accordingly!
 *
 * @param envClass  New environmental sound class.
 */
void Material_SetEnvClass(material_t* mat, material_env_class_t envClass);

#endif /* LIBDENG_MATERIAL_H */
