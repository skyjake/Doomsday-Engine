/**
 * @file animgroups.h (Material) Animation groups.
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

#ifndef LIBDENG_RESOURCE_ANIMGROUPS_H
#define LIBDENG_RESOURCE_ANIMGROUPS_H

#include "Texture"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * (Material) Animation group frame.
 * @ingroup resource
 */
typedef struct animframe_s
{
    void *textureManifest;
    ushort tics;
    ushort randomTics;
} animframe_t;

/**
 * (Material) Animation group.
 * @ingroup resource
 */
typedef struct animgroup_s
{
    int id;
    int flags;
    int count;
    animframe_t *frames;
} animgroup_t;

/// @return  Number of animation/precache groups.
int R_AnimGroupCount(void);

/// To be called to destroy all animation groups when they are no longer needed.
void R_ClearAnimGroups(void);

/// @return  AnimGroup associated with @a animGroupNum else @c NULL
animgroup_t const *R_ToAnimGroup(int animGroupNum);

/// @return  @c true iff @a texture is linked to the identified @a animGroupNum.
boolean R_IsTextureInAnimGroup(Uri const *texture, int animGroupNum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_ANIMGROUPS_H */
