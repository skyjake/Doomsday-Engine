/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * rend_shadow.c: Map Object Shadows
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int useShadows = true;
static int shadowMaxRad = 80;
static int shadowMaxDist = 1000;
static float shadowFactor = 0.5f;

// CODE --------------------------------------------------------------------

void Rend_ShadowRegister(void)
{
    // Cvars
    C_VAR_INT("rend-shadow", &useShadows, 0, 0, 1);
    C_VAR_FLOAT("rend-shadow-darkness", &shadowFactor, 0, 0, 1);
    C_VAR_INT("rend-shadow-far", &shadowMaxDist, CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-shadow-radius-max", &shadowMaxRad, CVF_NO_MAX, 0, 0);
}

/**
 * This is called for each sector a shadow-caster is touching.
 * The highest floor height will be searched and if larger than the
 * value of 'data' it will be written back.
 *
 * @param sector        The sector to search the floor height of.
 * @param data          Ptr to plane_t for comparison.
 *
 * @return              @c true, if iteration should continue.
 */
static int Rend_ShadowIterator(sector_t *sector, void *data)
{
    plane_t           **highest = (plane_t**) data;
    plane_t            *compare = sector->SP_plane(PLN_FLOOR);

    if(compare->visHeight > (*highest)->visHeight)
        *highest = compare;
    return false; // Continue iteration.
}

static void processMobjShadow(mobj_t* mo)
{
#define SHADOWZOFFSET       (0.2f)

    float               moz;
    float               height, moh, halfmoh, alpha, pos[2];
    sector_t*           sec = mo->subsector->sector;
    float               radius;
    uint                i;
    rvertex_t           rvertices[4];
    rcolor_t            rcolors[4];
    rtexcoord_t         rtexcoords[4];
    rtexmapunit_t       rTU[NUM_TEXMAP_UNITS];
    plane_t*            plane;
    float               distance;

    // Is this too far?
    pos[VX] = mo->pos[VX];
    pos[VY] = mo->pos[VY];
    if((distance = Rend_PointDist2D(pos)) > shadowMaxDist)
        return;

    // Apply a Short Range Visual Offset?
    if(useSRVO && mo->state && mo->tics >= 0)
    {
        float               mul = mo->tics / (float) mo->state->tics;

        pos[VX] += mo->srvo[VX] * mul;
        pos[VY] += mo->srvo[VY] * mul;
    }

    // Check the height.
    moz = mo->pos[VZ] - mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
        moz -= R_GetBobOffset(mo);

    height = moz - mo->floorZ;
    moh = mo->height;
    if(!moh)
        moh = 1;
    if(height > moh)
        return; // Too far.
    if(moz + mo->height < mo->floorZ)
        return;

    // Calculate the strength of the shadow.
    // Simplified version, no light diminishing or range compression.
    alpha = shadowFactor * sec->lightLevel *
        (1 - mo->translucency * reciprocal255);

    halfmoh = moh / 2;
    if(height > halfmoh)
        alpha *= 1 - (height - halfmoh) / (moh - halfmoh);
    if(usingFog)
        alpha /= 2;
    if(distance > 3 * shadowMaxDist / 4)
    {
        // Fade when nearing the maximum distance.
        alpha *= (shadowMaxDist - distance) / (shadowMaxDist / 4);
    }
    if(alpha <= 0)
        return; // Can't be seen.
    if(alpha > 1)
        alpha = 1;

    // Calculate the radius of the shadow.
    radius = R_VisualRadius(mo);
    if(!(radius > 0))
        return;
    if(radius > (float) shadowMaxRad)
        radius = (float) shadowMaxRad;

    // Figure out the visible floor height.
    plane = mo->subsector->sector->SP_plane(PLN_FLOOR);
    P_MobjSectorsIterator(mo, Rend_ShadowIterator, &plane);

    if(plane->visHeight >= moz + mo->height)
        return; // Can't have a shadow above the object!

    // View height might prevent us from seeing the shadow.
    if(vy < plane->visHeight)
        return;

    // Don't render mobj shadows on sky floors or glowing surfaces.
    if(R_IsGlowingPlane(plane))
        return;

    // Prepare the poly.
    memset(rTU, 0, sizeof(rTU));
    rTU[TU_PRIMARY].tex = GL_PrepareLSTexture(LST_DYNAMIC);
    rTU[TU_PRIMARY].magMode = GL_LINEAR;
    rTU[TU_PRIMARY].blend = 1;

    rvertices[0].pos[VX] = pos[VX] - radius;
    rvertices[0].pos[VY] = pos[VY] + radius;
    rvertices[0].pos[VZ] = plane->visHeight + SHADOWZOFFSET;
    rtexcoords[0].st[0] = 0;
    rtexcoords[0].st[1] = 1;

    rvertices[1].pos[VX] = pos[VX] + radius;
    rvertices[1].pos[VY] = pos[VY] + radius;
    rvertices[1].pos[VZ] = plane->visHeight + SHADOWZOFFSET;
    rtexcoords[1].st[0] = 1;
    rtexcoords[1].st[1] = 1;

    rvertices[2].pos[VX] = pos[VX] + radius;
    rvertices[2].pos[VY] = pos[VY] - radius;
    rvertices[2].pos[VZ] = plane->visHeight + SHADOWZOFFSET;
    rtexcoords[2].st[0] = 1;
    rtexcoords[2].st[1] = 0;

    rvertices[3].pos[VX] = pos[VX] - radius;
    rvertices[3].pos[VY] = pos[VY] - radius;
    rvertices[3].pos[VZ] = plane->visHeight + SHADOWZOFFSET;
    rtexcoords[3].st[0] = 0;
    rtexcoords[3].st[1] = 0;

    // Shadows are black.
    for(i = 0; i < 4; ++i)
    {
        rcolors[i].rgba[CR] = rcolors[i].rgba[CG] = rcolors[i].rgba[CB] = 0;
        rcolors[i].rgba[CA] = alpha;
    }

    RL_AddPoly(PT_FAN, RPT_SHADOW, rvertices, rtexcoords, NULL, NULL,
               rcolors, 4, 0, 0, NULL, rTU);

#undef SHADOWZOFFSET
}

void Rend_RenderShadows(void)
{
    sector_t               *sec;
    mobj_t                 *mo;
    uint                    i;

    if(!useShadows || levelFullBright)
        return;

    // Check all mobjs in all visible sectors.
    for(i = 0; i < numSectors; ++i)
    {
        sec = SECTOR_PTR(i);
        if(!(sec->frameFlags & SIF_VISIBLE))
            continue;

        for(mo = sec->mobjList; mo; mo = mo->sNext)
        {
            if(!mo->state)
                continue;

            // Should this mobj have a shadow?
            if((mo->state->flags & STF_FULLBRIGHT) || (mo->ddFlags & DDMF_DONTDRAW) ||
               (mo->ddFlags & DDMF_ALWAYSLIT))
                continue;

            processMobjShadow(mo);
        }
    }
}
