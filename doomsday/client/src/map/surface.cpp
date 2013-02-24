/** @file surface.cpp Logical map surface.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_play.h"
#include "de_defs.h"
#include "Materials"
#include <de/LegacyCore>
#include <de/Log>
#include <de/String>

using namespace de;

Surface::Surface() : de::MapElement(DMU_SURFACE)
{
    memset(&base, 0, sizeof(base));
    owner = 0;
    flags = 0;
    oldFlags = 0;
    material = 0;
    blendMode = BM_NORMAL;
    memset(tangent, 0, sizeof(tangent));
    memset(bitangent, 0, sizeof(bitangent));
    memset(normal, 0, sizeof(normal));
    memset(offset, 0, sizeof(offset));
    memset(oldOffset, 0, sizeof(oldOffset));
    memset(visOffset, 0, sizeof(visOffset));
    memset(visOffsetDelta, 0, sizeof(visOffsetDelta));
    memset(rgba, 1, sizeof(rgba));
    inFlags = 0;
    numDecorations = 0;
    decorations = 0;
}

Surface::~Surface()
{
}

bool Surface::isDrawable() const
{
    return material && material->isDrawable();
}

bool Surface::isSkyMasked() const
{
    return material && material->isSkyMasked();
}

bool Surface::isAttachedToMap() const
{
    if(!owner) return false;
    if(owner->type() == DMU_PLANE)
    {
        Sector *sec = owner->castTo<Plane>()->sector;
        if(0 == sec->bspLeafCount)
            return false;
    }
    return true;
}

bool Surface::setMaterial(Material *newMaterial)
{
    if(material != newMaterial)
    {
        if(isAttachedToMap())
        {
            // No longer a missing texture fix?
            if(newMaterial && (oldFlags & SUIF_FIX_MISSING_MATERIAL))
                inFlags &= ~SUIF_FIX_MISSING_MATERIAL;

            if(!ddMapSetup)
            {
#ifdef __CLIENT__
                GameMap *map = theMap; /// @todo Do not assume surface is from the CURRENT map.

                // If this plane's surface is in the decorated list, remove it.
                map->decoratedSurfaces().remove(this);

                // If this plane's surface is in the glowing list, remove it.
                map->glowingSurfaces().remove(this);
#endif // __CLIENT__

                if(newMaterial)
                {
#ifdef __CLIENT__
                    if(newMaterial->hasGlow())
                    {
                        map->glowingSurfaces().insert(this);
                    }

                    if(newMaterial->isDecorated())
                    {
                        map->decoratedSurfaces().insert(this);
                    }
#endif // __CLIENT__

                    if(owner->type() == DMU_PLANE)
                    {
                        de::Uri uri = newMaterial->manifest().composeUri();
                        ded_ptcgen_t const *def = Def_GetGenerator(reinterpret_cast<uri_s *>(&uri));
                        P_SpawnPlaneParticleGen(def, owner->castTo<Plane>());
                    }
                }
            }
        }

        material = newMaterial;
        oldFlags = inFlags;
        if(isAttachedToMap())
        {
            inFlags |= SUIF_UPDATE_DECORATIONS;
        }
    }
    return true;
}

bool Surface::setMaterialOriginX(float x)
{
    if(offset[VX] != x)
    {
        offset[VX] = x;
        if(isAttachedToMap())
        {
            inFlags |= SUIF_UPDATE_DECORATIONS;
            if(!ddMapSetup)
            {
                /// @todo Do not assume surface is from the CURRENT map.
                theMap->scrollingSurfaces().insert(this);
            }
        }
    }
    return true;
}

bool Surface::setMaterialOriginY(float y)
{
    if(offset[VY] != y)
    {
        offset[VY] = y;
        if(isAttachedToMap())
        {
            inFlags |= SUIF_UPDATE_DECORATIONS;
            if(!ddMapSetup)
            {
                /// @todo Do not assume surface is from the CURRENT map.
                theMap->scrollingSurfaces().insert(this);
            }
        }
    }
    return true;
}

bool Surface::setMaterialOrigin(float x, float y)
{
    if(offset[VX] != x || offset[VY] != y)
    {
        offset[VX] = x;
        offset[VY] = y;
        if(isAttachedToMap())
        {
            inFlags |= SUIF_UPDATE_DECORATIONS;
            if(!ddMapSetup)
            {
                /// @todo Do not assume surface is from the CURRENT map.
                theMap->scrollingSurfaces().insert(this);
            }
        }
    }
    return true;
}

bool Surface::setColorRed(float r)
{
    r = de::clamp(0.f, r, 1.f);
    if(rgba[CR] != r)
    {
        /// @todo when surface colours are intergrated with the
        /// bias lighting model we will need to recalculate the
        /// vertex colours when they are changed.
        rgba[CR] = r;
    }
    return true;
}

bool Surface::setColorGreen(float g)
{
    g = de::clamp(0.f, g, 1.f);
    if(rgba[CG] != g)
    {
        /// @todo when surface colours are intergrated with the
        /// bias lighting model we will need to recalculate the
        /// vertex colours when they are changed.
        rgba[CG] = g;
    }
    return true;
}

bool Surface::setColorBlue(float b)
{
    b = de::clamp(0.f, b, 1.f);
    if(rgba[CB] != b)
    {
        /// @todo when surface colours are intergrated with the
        /// bias lighting model we will need to recalculate the
        /// vertex colours when they are changed.
        rgba[CB] = b;
    }
    return true;
}

bool Surface::setAlpha(float a)
{
    a = de::clamp(0.f, a, 1.f);
    if(rgba[CA] != a)
    {
        /// @todo when surface colours are intergrated with the
        /// bias lighting model we will need to recalculate the
        /// vertex colours when they are changed.
        rgba[CA] = a;
    }
    return true;
}

bool Surface::setColorAndAlpha(float r, float g, float b, float a)
{
    r = de::clamp(0.f, r, 1.f);
    g = de::clamp(0.f, g, 1.f);
    b = de::clamp(0.f, b, 1.f);
    a = de::clamp(0.f, a, 1.f);

    if(rgba[CR] == r && rgba[CG] == g && rgba[CB] == b && rgba[CA] == a)
        return true;

    /// @todo when surface colours are intergrated with the
    /// bias lighting model we will need to recalculate the
    /// vertex colours when they are changed.
    rgba[CR] = r;
    rgba[CG] = g;
    rgba[CB] = b;
    rgba[CA] = a;

    return true;
}

bool Surface::setBlendMode(blendmode_t newBlendMode)
{
    if(blendMode != newBlendMode)
    {
        blendMode = newBlendMode;
    }
    return true;
}

void Surface::update()
{
    if(!isAttachedToMap()) return;
    inFlags |= SUIF_UPDATE_DECORATIONS;
}

void Surface::updateBaseOrigin()
{
    LOG_AS("Surface::updateBaseOrigin");

    if(!owner) return;
    switch(owner->type())
    {
    case DMU_PLANE: {
        Plane *pln = owner->castTo<Plane>();
        Sector *sec = pln->sector;
        DENG_ASSERT(sec);

        base.origin[VX] = sec->base.origin[VX];
        base.origin[VY] = sec->base.origin[VY];
        base.origin[VZ] = pln->height;
        break; }

    case DMU_SIDEDEF: {
        SideDef *side = owner->castTo<SideDef>();
        LineDef *line = side->line;
        Sector *sec;
        DENG_ASSERT(line);

        base.origin[VX] = (line->L_v1origin[VX] + line->L_v2origin[VX]) / 2;
        base.origin[VY] = (line->L_v1origin[VY] + line->L_v2origin[VY]) / 2;

        sec = line->L_sector(side == line->L_frontsidedef? FRONT:BACK);
        if(sec)
        {
            coord_t const ffloor = sec->SP_floorheight;
            coord_t const fceil  = sec->SP_ceilheight;

            if(this == &side->SW_middlesurface)
            {
                if(!line->L_backsidedef || LINE_SELFREF(line))
                    base.origin[VZ] = (ffloor + fceil) / 2;
                else
                    base.origin[VZ] = (MAX_OF(ffloor, line->L_backsector->SP_floorheight) +
                                       MIN_OF(fceil,  line->L_backsector->SP_ceilheight)) / 2;
                break;
            }
            else if(this == &side->SW_bottomsurface)
            {
                if(!line->L_backsidedef || LINE_SELFREF(line) ||
                   line->L_backsector->SP_floorheight <= ffloor)
                    base.origin[VZ] = ffloor;
                else
                    base.origin[VZ] = (MIN_OF(line->L_backsector->SP_floorheight, fceil) + ffloor) / 2;
                break;
            }
            else if(this == &side->SW_topsurface)
            {
                if(!line->L_backsidedef || LINE_SELFREF(line) ||
                   line->L_backsector->SP_ceilheight >= fceil)
                    base.origin[VZ] = fceil;
                else
                    base.origin[VZ] = (MAX_OF(line->L_backsector->SP_ceilheight, ffloor) + fceil) / 2;
                break;
            }
        }

        // We cannot determine a better Z axis origin - set to 0.
        base.origin[VZ] = 0;
        break; }

    default:
        LOG_DEBUG("Invalid DMU type %s for owner object %p.")
            << DMU_Str(owner->type()) << de::dintptr(owner);
        DENG2_ASSERT(false);
    }
}

#ifdef __CLIENT__
Surface::DecorSource *Surface::newDecoration()
{
    Surface::DecorSource *newDecorations =
        (DecorSource *) Z_Malloc(sizeof(*newDecorations) * (++numDecorations), PU_MAP, 0);

    if(numDecorations > 1)
    {
        // Copy the existing decorations.
        for(uint i = 0; i < numDecorations - 1; ++i)
        {
            Surface::DecorSource *d = &newDecorations[i];
            Surface::DecorSource *s = &((DecorSource *)decorations)[i];

            std::memcpy(d, s, sizeof(*d));
        }

        Z_Free(decorations);
    }

    Surface::DecorSource *d = &newDecorations[numDecorations - 1];
    decorations = (surfacedecorsource_s *)newDecorations;

    return d;
}

void Surface::clearDecorations()
{
    if(decorations)
    {
        Z_Free(decorations); decorations = 0;
    }
    numDecorations = 0;
}
#endif // __CLIENT__

int Surface::setProperty(setargs_t const &args)
{
    switch(args.prop)
    {
    case DMU_BLENDMODE: {
        blendmode_t newBlendMode;
        DMU_SetValue(DMT_SURFACE_BLENDMODE, &newBlendMode, &args, 0);
        setBlendMode(newBlendMode);
        break; }

    case DMU_FLAGS:
        DMU_SetValue(DMT_SURFACE_FLAGS, &flags, &args, 0);
        break;

    case DMU_COLOR: {
        float rgb[3];
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CR], &args, 0);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CG], &args, 1);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CB], &args, 2);
        setColorRed(rgb[CR]);
        setColorGreen(rgb[CG]);
        setColorBlue(rgb[CB]);
        break; }

    case DMU_COLOR_RED: {
        float r;
        DMU_SetValue(DMT_SURFACE_RGBA, &r, &args, 0);
        setColorRed(r);
        break; }

    case DMU_COLOR_GREEN: {
        float g;
        DMU_SetValue(DMT_SURFACE_RGBA, &g, &args, 0);
        setColorGreen(g);
        break; }

    case DMU_COLOR_BLUE: {
        float b;
        DMU_SetValue(DMT_SURFACE_RGBA, &b, &args, 0);
        setColorBlue(b);
        break; }

    case DMU_ALPHA: {
        float a;
        DMU_SetValue(DMT_SURFACE_RGBA, &a, &args, 0);
        setAlpha(a);
        break; }

    case DMU_MATERIAL: {
        Material *mat;
        DMU_SetValue(DMT_SURFACE_MATERIAL, &mat, &args, 0);
        setMaterial(mat);
        break; }

    case DMU_OFFSET_X: {
        float offX;
        DMU_SetValue(DMT_SURFACE_OFFSET, &offX, &args, 0);
        setMaterialOriginX(offX);
        break; }

    case DMU_OFFSET_Y: {
        float offY;
        DMU_SetValue(DMT_SURFACE_OFFSET, &offY, &args, 0);
        setMaterialOriginY(offY);
        break; }

    case DMU_OFFSET_XY: {
        float newOffset[2];
        DMU_SetValue(DMT_SURFACE_OFFSET, &newOffset[VX], &args, 0);
        DMU_SetValue(DMT_SURFACE_OFFSET, &newOffset[VY], &args, 1);
        setMaterialOrigin(newOffset[VX], newOffset[VY]);
        break; }

    default:
        String msg = String("Surface::setProperty: Property '%s' is not writable.").arg(DMU_Str(args.prop));
        LegacyCore_FatalError(msg.toUtf8().constData());
    }

    return false; // Continue iteration.
}

int Surface::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_BASE: {
        DMU_GetValue(DMT_SURFACE_BASE, &base, &args, 0);
        break; }

    case DMU_MATERIAL: {
        Material *mat = material;
        if(inFlags & SUIF_FIX_MISSING_MATERIAL)
            mat = NULL;
        DMU_GetValue(DMT_SURFACE_MATERIAL, &mat, &args, 0);
        break; }

    case DMU_OFFSET_X:
        DMU_GetValue(DMT_SURFACE_OFFSET, &offset[VX], &args, 0);
        break;

    case DMU_OFFSET_Y:
        DMU_GetValue(DMT_SURFACE_OFFSET, &offset[VY], &args, 0);
        break;

    case DMU_OFFSET_XY:
        DMU_GetValue(DMT_SURFACE_OFFSET, &offset[VX], &args, 0);
        DMU_GetValue(DMT_SURFACE_OFFSET, &offset[VY], &args, 1);
        break;

    case DMU_TANGENT_X:
        DMU_GetValue(DMT_SURFACE_TANGENT, &tangent[VX], &args, 0);
        break;

    case DMU_TANGENT_Y:
        DMU_GetValue(DMT_SURFACE_TANGENT, &tangent[VY], &args, 0);
        break;

    case DMU_TANGENT_Z:
        DMU_GetValue(DMT_SURFACE_TANGENT, &tangent[VZ], &args, 0);
        break;

    case DMU_TANGENT_XYZ:
        DMU_GetValue(DMT_SURFACE_TANGENT, &tangent[VX], &args, 0);
        DMU_GetValue(DMT_SURFACE_TANGENT, &tangent[VY], &args, 1);
        DMU_GetValue(DMT_SURFACE_TANGENT, &tangent[VZ], &args, 2);
        break;

    case DMU_BITANGENT_X:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &bitangent[VX], &args, 0);
        break;

    case DMU_BITANGENT_Y:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &bitangent[VY], &args, 0);
        break;

    case DMU_BITANGENT_Z:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &bitangent[VZ], &args, 0);
        break;

    case DMU_BITANGENT_XYZ:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &bitangent[VX], &args, 0);
        DMU_GetValue(DMT_SURFACE_BITANGENT, &bitangent[VY], &args, 1);
        DMU_GetValue(DMT_SURFACE_BITANGENT, &bitangent[VZ], &args, 2);
        break;

    case DMU_NORMAL_X:
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VX], &args, 0);
        break;

    case DMU_NORMAL_Y:
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VY], &args, 0);
        break;

    case DMU_NORMAL_Z:
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VZ], &args, 0);
        break;

    case DMU_NORMAL_XYZ:
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VX], &args, 0);
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VY], &args, 1);
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VZ], &args, 2);
        break;

    case DMU_COLOR:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CR], &args, 0);
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CG], &args, 1);
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CB], &args, 2);
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CA], &args, 2);
        break;

    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CR], &args, 0);
        break;

    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CG], &args, 0);
        break;

    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CB], &args, 0);
        break;

    case DMU_ALPHA:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CA], &args, 0);
        break;

    case DMU_BLENDMODE:
        DMU_GetValue(DMT_SURFACE_BLENDMODE, &blendMode, &args, 0);
        break;

    case DMU_FLAGS:
        DMU_GetValue(DMT_SURFACE_FLAGS, &flags, &args, 0);
        break;

    default:
        String msg = String("Surface::getProperty: Surface has no property '%s'.").arg(DMU_Str(args.prop));
        LegacyCore_FatalError(msg.toUtf8().constData());
    }

    return false; // Continue iteration.
}
