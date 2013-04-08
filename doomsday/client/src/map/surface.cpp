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

#include <de/LegacyCore>
#include <de/Log>
#include <de/String>

#include "de_base.h"
#include "de_play.h"
#include "de_defs.h"
#include "Materials"
#include "map/gamemap.h"

#include "map/surface.h"

using namespace de;

DENG2_PIMPL(Surface)
{
    /// Owning map element, either @c DMU_SIDEDEF, or @c DMU_PLANE.
    de::MapElement &owner;

    /// Sound emitter.
    ddmobj_base_t soundEmitter;

    /// Bound material.
    Material *material;

    /// @c true= Bound material is a "missing material fix".
    bool materialIsMissingFix;

    /// Surface color tint.
    de::Vector3f tintColor;

    /// Surface opacity.
    float opacity;

    /// Blending mode.
    blendmode_t blendMode;

    /// @ref sufFlags
    int flags;

    Instance(Public *i, MapElement &owner_, de::Vector3f const &tintColor,
             float opacity)
        : Base(i),
          owner(owner_),
          material(0),
          materialIsMissingFix(false),
          tintColor(tintColor),
          opacity(opacity),
          blendMode(BM_NORMAL),
          flags(0)
    {
        std::memset(&soundEmitter, 0, sizeof(soundEmitter));
    }

    void notifyOpacityChanged(float oldOpacity)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OpacityChange, i)
        {
            i->opacityChanged(self, oldOpacity);
        }
    }

    void notifyTintColorChanged(Vector3f const &oldTintColor)
    {
        // Predetermine which components have changed.
        int changedComponents = 0;
        for(int i = 0; i < 3; ++i)
        {
            if(!de::fequal(tintColor[i], oldTintColor[i]))
                changedComponents |= (1 << i);
        }

        DENG2_FOR_PUBLIC_AUDIENCE(TintColorChange, i)
        {
            i->tintColorChanged(self, oldTintColor, changedComponents);
        }
    }
};

Surface::Surface(MapElement &owner, Vector3f const &tintColor, float opacity)
    : MapElement(DMU_SURFACE), d(new Instance(this, owner, tintColor, opacity))
{
    V3f_Set(_tangent, 0, 0, 0);
    V3f_Set(_bitangent, 0, 0, 0);
    V3f_Set(_normal, 0, 0, 0);
    V2f_Set(_offset, 0, 0);
    V2f_Set(_oldOffset[0], 0, 0);
    V2f_Set(_oldOffset[1], 0, 0);
    V2f_Set(_visOffset, 0, 0);
    V2f_Set(_visOffsetDelta, 0, 0);

#ifdef __CLIENT__
    _decorationData.needsUpdate = false;
    _decorationData.sources     = 0;
    _decorationData.numSources  = 0;
#endif
}

de::MapElement &Surface::owner() const
{
    return d->owner;
}

const_pvec3f_t &Surface::tangent() const
{
    return _tangent;
}

const_pvec3f_t &Surface::bitangent() const
{
    return _bitangent;
}

const_pvec3f_t &Surface::normal() const
{
    return _normal;
}

bool Surface::hasMaterial() const
{
    return d->material != 0;
}

bool Surface::hasFixMaterial() const
{
    return d->material != 0 && d->materialIsMissingFix;
}

Material &Surface::material() const
{
    if(d->material)
    {
        return *d->material;
    }
    /// @throw MissingMaterialError Attempted with no material bound.
    throw MissingMaterialError("Surface::material", "No material is bound");
}

int Surface::flags() const
{
    return d->flags;
}

ddmobj_base_t &Surface::soundEmitter()
{
    return d->soundEmitter;
}

ddmobj_base_t const &Surface::soundEmitter() const
{
    return const_cast<ddmobj_base_t const &>(const_cast<Surface &>(*this).soundEmitter());
}

bool Surface::isAttachedToMap() const
{
    if(d->owner.type() == DMU_PLANE)
    {
        Sector const &sector = d->owner.castTo<Plane>()->sector();
        if(!sector.bspLeafCount())
            return false;
    }
    return true;
}

bool Surface::setMaterial(Material *newMaterial, bool isMissingFix)
{
    if(d->material != newMaterial)
    {
        // Update the missing-material-fix state.
        if(!d->material)
        {
            if(newMaterial && isMissingFix)
               d->materialIsMissingFix = true;
        }
        else if(newMaterial && d->materialIsMissingFix)
        {
            d->materialIsMissingFix = false;
        }

        if(isAttachedToMap())
        {
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

                    if(d->owner.type() == DMU_PLANE)
                    {
                        de::Uri uri = newMaterial->manifest().composeUri();
                        ded_ptcgen_t const *def = Def_GetGenerator(reinterpret_cast<uri_s *>(&uri));
                        P_SpawnPlaneParticleGen(def, d->owner.castTo<Plane>());
                    }
                }
            }
        }

        d->material = newMaterial;

#ifdef __CLIENT__
        if(isAttachedToMap())
        {
            /// @todo Replace with a de::Observer-based mechanism.
            _decorationData.needsUpdate = true;
        }
#endif
    }
    return true;
}

const_pvec2f_t &Surface::materialOrigin() const
{
    return _offset;
}

bool Surface::setMaterialOrigin(const_pvec2f_t newOrigin)
{
    if(_offset[VX] != newOrigin[VX] || _offset[VY] != newOrigin[VY])
    {
        V2f_Copy(_offset, newOrigin);

        if(isAttachedToMap())
        {
#ifdef __CLIENT__
            /// @todo Replace with a de::Observer-based mechanism.
            _decorationData.needsUpdate = true;
#endif

            if(!ddMapSetup)
            {
                /// @todo Do not assume surface is from the CURRENT map.
                theMap->scrollingSurfaces().insert(this);
            }
        }
    }
    return true;
}

bool Surface::setMaterialOriginX(float x)
{
    if(_offset[VX] != x)
    {
        _offset[VX] = x;
        if(isAttachedToMap())
        {
#ifdef __CLIENT__
            /// @todo Replace with a de::Observer-based mechanism.
            _decorationData.needsUpdate = true;
#endif

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
    if(_offset[VY] != y)
    {
        _offset[VY] = y;
        if(isAttachedToMap())
        {
#ifdef __CLIENT__
            /// @todo Replace with a de::Observer-based mechanism.
            _decorationData.needsUpdate = true;
#endif

            if(!ddMapSetup)
            {
                /// @todo Do not assume surface is from the CURRENT map.
                theMap->scrollingSurfaces().insert(this);
            }
        }
    }
    return true;
}

const_pvec2f_t &Surface::visMaterialOrigin() const
{
    return _visOffset;
}

const_pvec2f_t &Surface::visMaterialOriginDelta() const
{
    return _visOffsetDelta;
}

float Surface::opacity() const
{
    return d->opacity;
}

void Surface::setOpacity(float newOpacity)
{
    newOpacity = de::clamp(0.f, newOpacity, 1.f);
    if(!de::fequal(d->opacity, newOpacity))
    {
        float oldOpacity = d->opacity;
        d->opacity = newOpacity;

        // Notify interested parties of the change.
        d->notifyOpacityChanged(oldOpacity);
    }
}

Vector3f const &Surface::tintColor() const
{
    return d->tintColor;
}

void Surface::setTintColor(Vector3f const &newTintColor_)
{
    Vector3f newTintColor = Vector3f(de::clamp(0.f, newTintColor_.x, 1.f),
                                     de::clamp(0.f, newTintColor_.y, 1.f),
                                     de::clamp(0.f, newTintColor_.z, 1.f));
    if(d->tintColor != newTintColor)
    {
        Vector3f oldTintColor = d->tintColor;
        d->tintColor = newTintColor;

        // Notify interested parties of the change.
        d->notifyTintColorChanged(oldTintColor);
    }
}

void Surface::setTintColorComponent(int component, float newStrength)
{
    DENG_ASSERT(component >= 0 && component < 3);
    newStrength = de::clamp(0.f, newStrength, 1.f);
    if(!de::fequal(d->tintColor[component], newStrength))
    {
        Vector3f oldTintColor = d->tintColor;
        d->tintColor[component] = newStrength;

        // Notify interested parties of the change.
        d->notifyTintColorChanged(oldTintColor);
    }
}

blendmode_t Surface::blendMode() const
{
    return d->blendMode;
}

bool Surface::setBlendMode(blendmode_t newBlendMode)
{
    if(d->blendMode != newBlendMode)
    {
        d->blendMode = newBlendMode;
    }
    return true;
}

void Surface::updateSoundEmitterOrigin()
{
    LOG_AS("Surface::updateSoundEmitterOrigin");

    switch(d->owner.type())
    {
    case DMU_PLANE: {
        Plane *plane = d->owner.castTo<Plane>();
        Sector &sector = plane->sector();

        d->soundEmitter.origin[VX] = sector.soundEmitter().origin[VX];
        d->soundEmitter.origin[VY] = sector.soundEmitter().origin[VY];
        d->soundEmitter.origin[VZ] = plane->height();
        break; }

    case DMU_SIDEDEF: {
        SideDef *sideDef = d->owner.castTo<SideDef>();
        LineDef &line = sideDef->line();

        d->soundEmitter.origin[VX] = (line.v1Origin()[VX] + line.v2Origin()[VX]) / 2;
        d->soundEmitter.origin[VY] = (line.v1Origin()[VY] + line.v2Origin()[VY]) / 2;

        Sector *sec = line.sectorPtr(sideDef == line.frontSideDefPtr()? FRONT:BACK);
        if(sec)
        {
            coord_t const ffloor = sec->floor().height();
            coord_t const fceil  = sec->ceiling().height();

            if(this == &sideDef->middle())
            {
                if(!line.hasBackSideDef() || line.isSelfReferencing())
                    d->soundEmitter.origin[VZ] = (ffloor + fceil) / 2;
                else
                    d->soundEmitter.origin[VZ] = (de::max(ffloor, line.backSector().floor().height()) +
                                                  de::min(fceil,  line.backSector().ceiling().height())) / 2;
                break;
            }
            else if(this == &sideDef->bottom())
            {
                if(!line.hasBackSideDef() || line.isSelfReferencing() ||
                   line.backSector().floor().height() <= ffloor)
                {
                    d->soundEmitter.origin[VZ] = ffloor;
                }
                else
                {
                    d->soundEmitter.origin[VZ] = (de::min(line.backSector().floor().height(), fceil) + ffloor) / 2;
                }
                break;
            }
            else if(this == &sideDef->top())
            {
                if(!line.hasBackSideDef() || line.isSelfReferencing() ||
                   line.backSector().ceiling().height() >= fceil)
                {
                    d->soundEmitter.origin[VZ] = fceil;
                }
                else
                {
                    d->soundEmitter.origin[VZ] = (de::max(line.backSector().ceiling().height(), ffloor) + fceil) / 2;
                }
                break;
            }
        }

        // We cannot determine a better Z axis origin - set to 0.
        d->soundEmitter.origin[VZ] = 0;
        break; }

    default:
        throw Error("Surface::updateSoundEmitterOrigin",
                        QString("Invalid DMU type %1 for owner object 0x%2")
                            .arg(DMU_Str(d->owner.type()))
                            .arg(de::dintptr(&d->owner), 0, 16));
    }
}

#ifdef __CLIENT__
Surface::DecorSource *Surface::newDecoration()
{
    Surface::DecorSource *newSources =
        (DecorSource *) Z_Malloc(sizeof(*newSources) * (++_decorationData.numSources), PU_MAP, 0);

    if(_decorationData.numSources > 1)
    {
        // Copy the existing decorations.
        for(uint i = 0; i < _decorationData.numSources - 1; ++i)
        {
            Surface::DecorSource *d = &newSources[i];
            Surface::DecorSource *s = &((DecorSource *)_decorationData.sources)[i];

            std::memcpy(d, s, sizeof(*d));
        }

        Z_Free(_decorationData.sources);
    }

    Surface::DecorSource *d = &newSources[_decorationData.numSources - 1];
    _decorationData.sources = (surfacedecorsource_s *)newSources;

    return d;
}

void Surface::clearDecorations()
{
    if(_decorationData.sources)
    {
        Z_Free(_decorationData.sources); _decorationData.sources = 0;
    }
    _decorationData.numSources = 0;
}

uint Surface::decorationCount() const
{
    return _decorationData.numSources;
}

void Surface::markAsNeedingDecorationUpdate()
{
    if(!isAttachedToMap()) return;

    /// @todo Replace with a de::Observer-based mechanism.
    _decorationData.needsUpdate = true;
}
#endif // __CLIENT__

int Surface::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_BASE: {
        DMU_GetValue(DMT_SURFACE_BASE, &d->soundEmitter, &args, 0);
        break; }

    case DMU_MATERIAL: {
        Material *mat = d->material;
        if(d->materialIsMissingFix)
            mat = 0;
        DMU_GetValue(DMT_SURFACE_MATERIAL, &mat, &args, 0);
        break; }

    case DMU_OFFSET_X:
        DMU_GetValue(DMT_SURFACE_OFFSET, &_offset[VX], &args, 0);
        break;

    case DMU_OFFSET_Y:
        DMU_GetValue(DMT_SURFACE_OFFSET, &_offset[VY], &args, 0);
        break;

    case DMU_OFFSET_XY:
        DMU_GetValue(DMT_SURFACE_OFFSET, &_offset[VX], &args, 0);
        DMU_GetValue(DMT_SURFACE_OFFSET, &_offset[VY], &args, 1);
        break;

    case DMU_TANGENT_X:
        DMU_GetValue(DMT_SURFACE_TANGENT, &_tangent[VX], &args, 0);
        break;

    case DMU_TANGENT_Y:
        DMU_GetValue(DMT_SURFACE_TANGENT, &_tangent[VY], &args, 0);
        break;

    case DMU_TANGENT_Z:
        DMU_GetValue(DMT_SURFACE_TANGENT, &_tangent[VZ], &args, 0);
        break;

    case DMU_TANGENT_XYZ:
        DMU_GetValue(DMT_SURFACE_TANGENT, &_tangent[VX], &args, 0);
        DMU_GetValue(DMT_SURFACE_TANGENT, &_tangent[VY], &args, 1);
        DMU_GetValue(DMT_SURFACE_TANGENT, &_tangent[VZ], &args, 2);
        break;

    case DMU_BITANGENT_X:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &_bitangent[VX], &args, 0);
        break;

    case DMU_BITANGENT_Y:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &_bitangent[VY], &args, 0);
        break;

    case DMU_BITANGENT_Z:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &_bitangent[VZ], &args, 0);
        break;

    case DMU_BITANGENT_XYZ:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &_bitangent[VX], &args, 0);
        DMU_GetValue(DMT_SURFACE_BITANGENT, &_bitangent[VY], &args, 1);
        DMU_GetValue(DMT_SURFACE_BITANGENT, &_bitangent[VZ], &args, 2);
        break;

    case DMU_NORMAL_X:
        DMU_GetValue(DMT_SURFACE_NORMAL, &_normal[VX], &args, 0);
        break;

    case DMU_NORMAL_Y:
        DMU_GetValue(DMT_SURFACE_NORMAL, &_normal[VY], &args, 0);
        break;

    case DMU_NORMAL_Z:
        DMU_GetValue(DMT_SURFACE_NORMAL, &_normal[VZ], &args, 0);
        break;

    case DMU_NORMAL_XYZ:
        DMU_GetValue(DMT_SURFACE_NORMAL, &_normal[VX], &args, 0);
        DMU_GetValue(DMT_SURFACE_NORMAL, &_normal[VY], &args, 1);
        DMU_GetValue(DMT_SURFACE_NORMAL, &_normal[VZ], &args, 2);
        break;

    case DMU_COLOR:
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor[CR], &args, 0);
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor[CG], &args, 1);
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor[CB], &args, 2);
        DMU_GetValue(DMT_SURFACE_RGBA, &d->opacity, &args, 2);
        break;

    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor[CR], &args, 0);
        break;

    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor[CG], &args, 0);
        break;

    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor[CB], &args, 0);
        break;

    case DMU_ALPHA:
        DMU_GetValue(DMT_SURFACE_RGBA, &d->opacity, &args, 0);
        break;

    case DMU_BLENDMODE:
        DMU_GetValue(DMT_SURFACE_BLENDMODE, &d->blendMode, &args, 0);
        break;

    case DMU_FLAGS:
        DMU_GetValue(DMT_SURFACE_FLAGS, &d->flags, &args, 0);
        break;

    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("Surface::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

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
        DMU_SetValue(DMT_SURFACE_FLAGS, &d->flags, &args, 0);
        break;

    case DMU_COLOR: {
        float rgb[3];
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CR], &args, 0);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CG], &args, 1);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CB], &args, 2);
        setTintRed(rgb[CR]);
        setTintGreen(rgb[CG]);
        setTintBlue(rgb[CB]);
        break; }

    case DMU_COLOR_RED: {
        float r;
        DMU_SetValue(DMT_SURFACE_RGBA, &r, &args, 0);
        setTintRed(r);
        break; }

    case DMU_COLOR_GREEN: {
        float g;
        DMU_SetValue(DMT_SURFACE_RGBA, &g, &args, 0);
        setTintGreen(g);
        break; }

    case DMU_COLOR_BLUE: {
        float b;
        DMU_SetValue(DMT_SURFACE_RGBA, &b, &args, 0);
        setTintBlue(b);
        break; }

    case DMU_ALPHA: {
        float a;
        DMU_SetValue(DMT_SURFACE_RGBA, &a, &args, 0);
        setOpacity(a);
        break; }

    case DMU_MATERIAL: {
        Material *newMaterial;
        DMU_SetValue(DMT_SURFACE_MATERIAL, &newMaterial, &args, 0);
        setMaterial(newMaterial);
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
        vec2f_t newOrigin;
        DMU_SetValue(DMT_SURFACE_OFFSET, &newOrigin[VX], &args, 0);
        DMU_SetValue(DMT_SURFACE_OFFSET, &newOrigin[VY], &args, 1);
        setMaterialOrigin(newOrigin);
        break; }

    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("Surface::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

bool Surface::isFromPolyobj(Surface const &surface)
{
    if(surface.owner().type() == DMU_SIDEDEF)
    {
        SideDef *sideDef = surface.owner().castTo<SideDef>();
        if(sideDef->line().isFromPolyobj()) return true;
    }
    return false;
}
