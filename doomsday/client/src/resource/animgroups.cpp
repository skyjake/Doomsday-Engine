/**
 * @file animgroups.cpp (Material) Animation groups
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

#include <cstring>

#include "de_base.h"
#include "de_console.h"
#include "de_resource.h"
#include <de/memoryzone.h>

using namespace de;

static int numgroups;
static animgroup_t *groups;

static animgroup_t *getAnimGroup(int number)
{
    if(--number < 0 || number >= numgroups) return 0;
    return &groups[number];
}

static bool isInAnimGroup(animgroup_t const &group, TextureManifest &manifest)
{
    for(int i = 0; i < group.count; ++i)
    {
        if(group.frames[i].textureManifest == &manifest)
            return true;
    }
    return false;
}

void R_ClearAnimGroups()
{
    if(numgroups <= 0) return;

    for(int i = 0; i < numgroups; ++i)
    {
        animgroup_t &grp = groups[i];
        if(grp.frames) Z_Free(grp.frames);
    }
    Z_Free(groups); groups = 0;
    numgroups = 0;
}

animgroup_t const *R_ToAnimGroup(int animGroupNum)
{
    LOG_AS("R_ToAnimGroup");
    animgroup_t *grp = getAnimGroup(animGroupNum);
    if(!grp) LOG_DEBUG("Invalid group #%i, returning NULL.") << animGroupNum;
    return grp;
}

int R_AnimGroupCount()
{
    return numgroups;
}

#undef R_CreateAnimGroup
DENG_EXTERN_C int R_CreateAnimGroup(int flags)
{
    // Allocating one by one is inefficient but it doesn't really matter.
    groups = (animgroup_t *) Z_Realloc(groups, sizeof(*groups) * ++numgroups, PU_APPSTATIC);
    if(!groups) Con_Error("R_CreateAnimGroup: Failed on (re)allocation of %lu bytes enlarging AnimGroup list.", (unsigned long) sizeof(*groups) * numgroups);

    animgroup_t *group = &groups[numgroups-1];

    // Init the new group.
    std::memset(group, 0, sizeof *group);
    group->id = numgroups; // 1-based index.
    group->flags = flags;

    return group->id;
}

#undef R_AddAnimGroupFrame
DENG_EXTERN_C void R_AddAnimGroupFrame(int groupNum, uri_s const *textureUri, int tics, int randomTics)
{
    LOG_AS("R_AddAnimGroupFrame");

    if(!textureUri) return;

    animgroup_t *group = getAnimGroup(groupNum);
    if(!group)
    {
        LOG_DEBUG("Unknown anim group #%i, ignoring.") << groupNum;
        return;
    }

    try
    {
        TextureManifest &manifest = App_Textures()->find(reinterpret_cast<de::Uri const &>(*textureUri));

        // Allocate a new animframe.
        group->frames = (animframe_t *) Z_Realloc(group->frames, sizeof(*group->frames) * ++group->count, PU_APPSTATIC);
        if(!group->frames) Con_Error("R_AddAnimGroupFrame: Failed on (re)allocation of %lu bytes enlarging AnimFrame list for group #%i.", (unsigned long) sizeof(*group->frames) * group->count, groupNum);

        animframe_t *frame = &group->frames[group->count - 1];
        frame->textureManifest = &manifest;
        frame->tics = tics;
        frame->randomTics = randomTics;
    }
    catch(Textures::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ". Failed adding texture \"%s\" to group #%i, ignoring.")
            << reinterpret_cast<de::Uri const &>(*textureUri) << groupNum;
    }
}

boolean R_IsTextureInAnimGroup(uri_s const *textureUri, int groupNum)
{
    if(!textureUri) return false;
    animgroup_t *group = getAnimGroup(groupNum);
    if(!group) return false;

    try
    {
        TextureManifest &manifest = App_Textures()->find(reinterpret_cast<de::Uri const &>(*textureUri));
        return isInAnimGroup(*group, manifest);
    }
    catch(Textures::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return false;
}
