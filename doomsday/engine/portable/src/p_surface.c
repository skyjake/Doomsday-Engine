/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * r_surface.c: World surfaces.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Change the material to be used on the specified surface.
 *
 * @param suf           Ptr to the surface to chage the material of.
 * @param mat           Ptr to the material to change to.
 * @return              @c true, if changed successfully.
 */
boolean Surface_SetMaterial(surface_t* suf, material_t* mat)
{
    if(!suf || !mat)
        return false;

    if(suf->material == mat)
        return true;

    // No longer a missing texture fix?
    if(mat && (suf->oldFlags & SUIF_MATERIAL_FIX))
        suf->inFlags &= ~SUIF_MATERIAL_FIX;

    suf->material = mat;

    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
    suf->oldFlags = suf->inFlags;

    return true;
}

/**
 * Update the surface, material X offset.
 *
 * @param suf           The surface to be updated.
 * @param x             Material offset, X delta.
 *
 * @return              @c true, if the change was made successfully.
 */
boolean Surface_SetMaterialOffsetX(surface_t* suf, float x)
{
    if(!suf)
        return false;

    if(suf->offset[VX] == x)
        return true;

    suf->offset[VX] = x;
    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
    if(!ddMapSetup)
        R_SurfaceListAdd(movingSurfaceList, suf);

    return true;
}

/**
 * Update the surface, material Y offset.
 *
 * @param suf           The surface to be updated.
 * @param y             Material offset, Y delta.
 *
 * @return              @c true, if the change was made successfully.
 */
boolean Surface_SetMaterialOffsetY(surface_t* suf, float y)
{
    if(!suf)
        return false;

    if(suf->offset[VY] == y)
        return true;

    suf->offset[VY] = y;
    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
    if(!ddMapSetup)
        R_SurfaceListAdd(movingSurfaceList, suf);

    return true;
}

/**
 * Update the surface, material X+Y offset.
 *
 * @param suf           The surface to be updated.
 * @param x             Material offset, X delta.
 * @param y             Material offset, Y delta.
 *
 * @return              @c true, if the change was made successfully.
 */
boolean Surface_SetMaterialOffsetXY(surface_t* suf, float x, float y)
{
    if(!suf)
        return false;

    if(suf->offset[VX] == x && suf->offset[VY] == y)
        return true;

    suf->offset[VX] = x;
    suf->offset[VY] = y;
    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
    if(!ddMapSetup)
        R_SurfaceListAdd(movingSurfaceList, suf);

    return true;
}

/**
 * Update the surface, red color component.
 */
boolean Surface_SetColorR(surface_t* suf, float r)
{
    if(!suf)
        return false;

    r = MINMAX_OF(0, r, 1);

    if(suf->rgba[CR] == r)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CR] = r;

    return true;
}

/**
 * Update the surface, green color component.
 */
boolean Surface_SetColorG(surface_t* suf, float g)
{
    if(!suf)
        return false;

    g = MINMAX_OF(0, g, 1);

    if(suf->rgba[CG] == g)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CG] = g;

    return true;
}

/**
 * Update the surface, blue color component.
 */
boolean Surface_SetColorB(surface_t* suf, float b)
{
    if(!suf)
        return false;

    b = MINMAX_OF(0, b, 1);

    if(suf->rgba[CB] == b)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CB] = b;

    return true;
}

/**
 * Update the surface, alpha.
 */
boolean Surface_SetColorA(surface_t* suf, float a)
{
    if(!suf)
        return false;

    a = MINMAX_OF(0, a, 1);

    if(suf->rgba[CA] == a)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CA] = a;

    return true;
}

/**
 * Update the surface, color.
 */
boolean Surface_SetColorRGBA(surface_t* suf, float r, float g, float b,
                             float a)
{
    if(!suf)
        return false;

    r = MINMAX_OF(0, r, 1);
    g = MINMAX_OF(0, g, 1);
    b = MINMAX_OF(0, b, 1);
    a = MINMAX_OF(0, a, 1);

    if(suf->rgba[CR] == r && suf->rgba[CG] == g && suf->rgba[CB] == b &&
       suf->rgba[CA] == a)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CR] = r;
    suf->rgba[CG] = g;
    suf->rgba[CB] = b;
    suf->rgba[CA] = a;

    return true;
}

/**
 * Update the surface, blendmode.
 */
boolean Surface_SetBlendMode(surface_t* suf, blendmode_t blendMode)
{
    if(!suf)
        return false;

    if(suf->blendMode == blendMode)
        return true;

    suf->blendMode = blendMode;
    return true;
}

/**
 * Mark the surface as requiring a full update. Called during engine-reset.
 */
void Surface_Update(surface_t* suf)
{
    if(!suf)
        return;

    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
}

/**
 * Update the surface, property is selected by DMU_* name.
 */
boolean Surface_SetProperty(surface_t* suf, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_BLENDMODE:
        {
        blendmode_t             blendMode;
        DMU_SetValue(DMT_SURFACE_BLENDMODE, &blendMode, args, 0);
        Surface_SetBlendMode(suf, blendMode);
        }
        break;

    case DMU_FLAGS:
        DMU_SetValue(DMT_SURFACE_FLAGS, &suf->flags, args, 0);
        break;

    case DMU_COLOR:
        {
        float                   rgb[3];
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CR], args, 0);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CG], args, 1);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CB], args, 2);
        Surface_SetColorR(suf, rgb[CR]);
        Surface_SetColorG(suf, rgb[CG]);
        Surface_SetColorB(suf, rgb[CB]);
        }
        break;
    case DMU_COLOR_RED:
        {
        float           r;
        DMU_SetValue(DMT_SURFACE_RGBA, &r, args, 0);
        Surface_SetColorR(suf, r);
        }
        break;
    case DMU_COLOR_GREEN:
        {
        float           g;
        DMU_SetValue(DMT_SURFACE_RGBA, &g, args, 0);
        Surface_SetColorG(suf, g);
        }
        break;
    case DMU_COLOR_BLUE:
        {
        float           b;
        DMU_SetValue(DMT_SURFACE_RGBA, &b, args, 0);
        Surface_SetColorB(suf, b);
        }
        break;
    case DMU_ALPHA:
        {
        float           a;
        DMU_SetValue(DMT_SURFACE_RGBA, &a, args, 0);
        Surface_SetColorA(suf, a);
        }
        break;
    case DMU_MATERIAL:
        {
        material_t*     mat;
        DMU_SetValue(DMT_SURFACE_MATERIAL, &mat, args, 0);

        Surface_SetMaterial(suf, mat);
        }
        break;
    case DMU_OFFSET_X:
        {
        float           offX;
        DMU_SetValue(DMT_SURFACE_OFFSET, &offX, args, 0);
        Surface_SetMaterialOffsetX(suf, offX);
        }
        break;
    case DMU_OFFSET_Y:
        {
        float           offY;
        DMU_SetValue(DMT_SURFACE_OFFSET, &offY, args, 0);
        Surface_SetMaterialOffsetY(suf, offY);
        }
        break;
    case DMU_OFFSET_XY:
        {
        float           offset[2];
        DMU_SetValue(DMT_SURFACE_OFFSET, &offset[VX], args, 0);
        DMU_SetValue(DMT_SURFACE_OFFSET, &offset[VY], args, 1);
        Surface_SetMaterialOffsetXY(suf, offset[VX], offset[VY]);
        }
        break;
    default:
        Con_Error("Surface_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a surface property, selected by DMU_* name.
 */
boolean Surface_GetProperty(const surface_t *suf, setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_MATERIAL:
        {
        material_t*     mat = suf->material;

        if(suf->inFlags & SUIF_MATERIAL_FIX)
            mat = NULL;
        DMU_GetValue(DMT_SURFACE_MATERIAL, &mat, args, 0);
        break;
        }
    case DMU_OFFSET_X:
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VX], args, 0);
        break;
    case DMU_OFFSET_Y:
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VY], args, 0);
        break;
    case DMU_OFFSET_XY:
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VY], args, 1);
        break;
    case DMU_NORMAL_X:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VX], args, 0);
        break;
    case DMU_NORMAL_Y:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VY], args, 0);
        break;
    case DMU_NORMAL_Z:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VZ], args, 0);
        break;
    case DMU_NORMAL_XYZ:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VY], args, 1);
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VZ], args, 2);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CR], args, 0);
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CG], args, 1);
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CB], args, 2);
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CA], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CR], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CG], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CB], args, 0);
        break;
    case DMU_ALPHA:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CA], args, 0);
        break;
    case DMU_BLENDMODE:
        DMU_GetValue(DMT_SURFACE_BLENDMODE, &suf->blendMode, args, 0);
        break;
    case DMU_FLAGS:
        DMU_GetValue(DMT_SURFACE_FLAGS, &suf->flags, args, 0);
        break;
    default:
        Con_Error("Surface_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
