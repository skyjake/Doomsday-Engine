/** @file surface.cpp World Map Surface.
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

#include <de/memoryzone.h> /// @todo remove me

#include <de/Log>

#include "de_defs.h" // Def_GetGenerator

#include "Materials"
#include "world/map.h"
#include "world/r_world.h" /// ddMapSetup @todo remove me
#include "render/r_main.h" /// frameTimePos

#include "world/surface.h"

using namespace de;

float const Surface::DEFAULT_OPACITY = 1.f;
Vector3f const Surface::DEFAULT_TINT_COLOR = Vector3f(1.f, 1.f, 1.f);
int const Surface::MAX_SMOOTH_MATERIAL_MOVE = 8;

DENG2_PIMPL(Surface)
{
    /// Owning map element, either @c DMU_SIDE, or @c DMU_PLANE.
    de::MapElement &owner;

    /// Tangent space vectors:
    Vector3f tangent;
    Vector3f bitangent;
    Vector3f normal;

    bool needUpdateTangents;

    /// Bound material.
    Material *material;

    /// @c true= Bound material is a "missing material fix".
    bool materialIsMissingFix;

    /// @em Sharp origin of the surface material.
    Vector2f materialOrigin;

    /// Old @em sharp origin of the surface material, for smoothing.
    Vector2f oldMaterialOrigin[2];

    /// Smoothed origin of the surface material.
    Vector2f visMaterialOrigin;

    /// Delta between the @sharp and smoothed origin of the surface material.
    Vector2f visMaterialOriginDelta;

    /// Surface color tint.
    de::Vector3f tintColor;

    /// Surface opacity.
    float opacity;

    /// Blending mode.
    blendmode_t blendMode;

    /// @ref sufFlags
    int flags;

    Instance(Public *i, MapElement &owner)
        : Base(i),
          owner(owner),
          needUpdateTangents(false),
          material(0),
          materialIsMissingFix(false),
          blendMode(BM_NORMAL),
          flags(0)
    {}

    /// @todo Refactor away -ds
    inline bool isSideMiddle()
    {
        return owner.type() == DMU_SIDE && &self == &owner.castTo<Line::Side>()->middle();
    }

    /// @todo Refactor away -ds
    inline bool isSectorExtraPlane()
    {
        if(owner.type() != DMU_PLANE) return false;
        Plane const &plane = *owner.castTo<Plane>();
        return !(plane.isSectorFloor() || plane.isSectorCeiling());
    }

    void notifyNormalChanged(Vector3f const &oldNormal)
    {
        // Predetermine which axes have changed.
        int changedAxes = 0;
        for(int i = 0; i < 3; ++i)
        {
            if(!de::fequal(normal[i], oldNormal[i]))
                changedAxes |= (1 << i);
        }

        DENG2_FOR_PUBLIC_AUDIENCE(NormalChange, i)
        {
            i->normalChanged(self, oldNormal, changedAxes);
        }
    }

    void notifyMaterialOriginChanged(Vector2f const &oldMaterialOrigin,
                                     int changedComponents)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(MaterialOriginChange, i)
        {
            i->materialOriginChanged(self, oldMaterialOrigin, changedComponents);
        }

        if(!ddMapSetup && self.isAttachedToMap())
        {
#ifdef __CLIENT__
            /// @todo Replace with a de::Observer-based mechanism.
            self._decorationData.needsUpdate = true;
#endif
            /// @todo Do not assume surface is from the CURRENT map.
            theMap->scrollingSurfaces().insert(&self);
        }
    }

    void notifyMaterialOriginChanged(Vector2f const &oldMaterialOrigin)
    {
        // Predetermine which axes have changed.
        int changedAxes = 0;
        for(int i = 0; i < 2; ++i)
        {
            if(!de::fequal(materialOrigin[i], oldMaterialOrigin[i]))
                changedAxes |= (1 << i);
        }

        notifyMaterialOriginChanged(oldMaterialOrigin, changedAxes);
    }

    void notifyOpacityChanged(float oldOpacity)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OpacityChange, i)
        {
            i->opacityChanged(self, oldOpacity);
        }
    }

    void notifyTintColorChanged(Vector3f const &oldTintColor,
                                int changedComponents)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(TintColorChange, i)
        {
            i->tintColorChanged(self, oldTintColor, changedComponents);
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

        notifyTintColorChanged(oldTintColor, changedComponents);
    }

    void updateTangents()
    {
        vec3f_t v1Tangent, v1Bitangent;
        vec3f_t v1Normal; V3f_Set(v1Normal, normal.x, normal.y, normal.z);
        V3f_BuildTangents(v1Tangent, v1Bitangent, v1Normal);
        tangent = Vector3f(v1Tangent);
        bitangent = Vector3f(v1Bitangent);
        needUpdateTangents = false;
    }
};

Surface::Surface(MapElement &owner, float opacity, Vector3f const &tintColor)
    : MapElement(DMU_SURFACE), d(new Instance(this, owner))
{
#ifdef __CLIENT__
    _decorationData.needsUpdate = true;
    _decorationData.sources     = 0;
    _decorationData.numSources  = 0;
#endif

    d->opacity   = opacity;
    d->tintColor = tintColor;
}

de::MapElement &Surface::owner() const
{
    return d->owner;
}

de::Vector3f const &Surface::tangent() const
{
    if(d->needUpdateTangents)
        d->updateTangents();
    return d->tangent;
}

de::Vector3f const &Surface::bitangent() const
{
    if(d->needUpdateTangents)
        d->updateTangents();
    return d->bitangent;
}

de::Vector3f const &Surface::normal() const
{
    return d->normal;
}

void Surface::setNormal(Vector3f const &newNormal_)
{
    // Normalize
    Vector3f newNormal = newNormal_;
    dfloat length = newNormal.length();
    if(length)
    {
        for(int i = 0; i < 3; ++i)
            newNormal[i] /= length;
    }

    if(d->normal != newNormal)
    {
        Vector3f oldNormal = d->normal;
        d->normal = newNormal;

        // We'll need to recalculate the tangents when next referenced.
        d->needUpdateTangents = true;

        // Notify interested parties of the change.
        d->notifyNormalChanged(oldNormal);
    }
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
            {
                d->materialIsMissingFix = true;

                // Sides of selfreferencing map lines should never receive fix materials.
                DENG_ASSERT(!(d->owner.type() == DMU_SIDE && d->owner.castTo<Line::Side>()->line().isSelfReferencing()));
            }
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
                Map *map = theMap; /// @todo Do not assume surface is from the CURRENT map.

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

Vector2f const &Surface::materialOrigin() const
{
    return d->materialOrigin;
}

void Surface::setMaterialOrigin(Vector2f const &newOrigin)
{
    if(d->materialOrigin != newOrigin)
    {
        Vector2f oldMaterialOrigin = d->materialOrigin;

        d->materialOrigin = newOrigin;

        // During map setup we'll apply this immediately to the visual origin also.
        if(ddMapSetup)
        {
            d->visMaterialOrigin = d->materialOrigin;
            d->visMaterialOriginDelta.x = d->visMaterialOriginDelta.y = 0;

            d->oldMaterialOrigin[0] = d->oldMaterialOrigin[1] = d->materialOrigin;
        }

        // Notify interested parties of the change.
        d->notifyMaterialOriginChanged(oldMaterialOrigin);
    }
}

void Surface::setMaterialOriginComponent(int component, float newPosition)
{
    if(!de::fequal(d->materialOrigin[component], newPosition))
    {
        Vector2f oldMaterialOrigin = d->materialOrigin;

        d->materialOrigin[component] = newPosition;

        // During map setup we'll apply this immediately to the visual origin also.
        if(ddMapSetup)
        {
            d->visMaterialOrigin[component] = d->materialOrigin[component];
            d->visMaterialOriginDelta[component] = 0;

            d->oldMaterialOrigin[0][component] =
                d->oldMaterialOrigin[1][component] =
                    d->materialOrigin[component];
        }

        // Notify interested parties of the change.
        d->notifyMaterialOriginChanged(oldMaterialOrigin, (1 << component));
    }
}

Vector2f const &Surface::visMaterialOrigin() const
{
    return d->visMaterialOrigin;
}

Vector2f const &Surface::visMaterialOriginDelta() const
{
    return d->visMaterialOriginDelta;
}

void Surface::lerpVisMaterialOrigin()
{
    // $smoothmaterialorigin

    d->visMaterialOriginDelta = d->oldMaterialOrigin[0] * (1 - frameTimePos)
        + d->materialOrigin * frameTimePos - d->materialOrigin;

    // Visible material origin.
    d->visMaterialOrigin = d->materialOrigin + d->visMaterialOriginDelta;

#ifdef __CLIENT__
    markAsNeedingDecorationUpdate();
#endif
}

void Surface::resetVisMaterialOrigin()
{
    // $smoothmaterialorigin
    d->visMaterialOrigin = d->oldMaterialOrigin[0] = d->oldMaterialOrigin[1] = d->materialOrigin;
    d->visMaterialOriginDelta.x = d->visMaterialOriginDelta.y = 0;

#ifdef __CLIENT__
    markAsNeedingDecorationUpdate();
#endif
}

void Surface::updateMaterialOriginTracking()
{
    // $smoothmaterialorigin
    d->oldMaterialOrigin[0] = d->oldMaterialOrigin[1];
    d->oldMaterialOrigin[1] = d->materialOrigin;

    if(d->oldMaterialOrigin[0] != d->oldMaterialOrigin[1])
    {
        float moveDistance = de::abs(Vector2f(d->oldMaterialOrigin[1] - d->oldMaterialOrigin[0]).length());

        if(moveDistance >= MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            d->oldMaterialOrigin[0] = d->oldMaterialOrigin[1];
        }
    }
}

float Surface::opacity() const
{
    return d->opacity;
}

void Surface::setOpacity(float newOpacity)
{
    DENG_ASSERT(d->isSideMiddle() || d->isSectorExtraPlane());

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
        d->notifyTintColorChanged(oldTintColor, (1 << component));
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
    if(ddMapSetup || !isAttachedToMap()) return;

    _decorationData.needsUpdate = true;
}
#endif // __CLIENT__

int Surface::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_MATERIAL: {
        Material *mat = d->material;
        if(d->materialIsMissingFix)
            mat = 0;
        DMU_GetValue(DMT_SURFACE_MATERIAL, &mat, &args, 0);
        break; }

    case DMU_OFFSET_X:
        DMU_GetValue(DMT_SURFACE_OFFSET, &d->materialOrigin.x, &args, 0);
        break;

    case DMU_OFFSET_Y:
        DMU_GetValue(DMT_SURFACE_OFFSET, &d->materialOrigin.y, &args, 0);
        break;

    case DMU_OFFSET_XY:
        DMU_GetValue(DMT_SURFACE_OFFSET, &d->materialOrigin.x, &args, 0);
        DMU_GetValue(DMT_SURFACE_OFFSET, &d->materialOrigin.y, &args, 1);
        break;

    case DMU_TANGENT_X: {
        float x = tangent().x;
        DMU_GetValue(DMT_SURFACE_TANGENT, &x, &args, 0);
        break; }

    case DMU_TANGENT_Y: {
        float y = tangent().y;
        DMU_GetValue(DMT_SURFACE_TANGENT, &y, &args, 0);
        break; }

    case DMU_TANGENT_Z: {
        float z = tangent().z;
        DMU_GetValue(DMT_SURFACE_TANGENT, &z, &args, 0);
        break; }

    case DMU_TANGENT_XYZ: {
        Vector3f const &tan = tangent();
        DMU_GetValue(DMT_SURFACE_TANGENT, &tan.x, &args, 0);
        DMU_GetValue(DMT_SURFACE_TANGENT, &tan.y, &args, 1);
        DMU_GetValue(DMT_SURFACE_TANGENT, &tan.z, &args, 2);
        break; }

    case DMU_BITANGENT_X: {
        float x = bitangent().x;
        DMU_GetValue(DMT_SURFACE_BITANGENT, &x, &args, 0);
        break; }

    case DMU_BITANGENT_Y: {
        float y = bitangent().y;
        DMU_GetValue(DMT_SURFACE_BITANGENT, &y, &args, 0);
        break; }

    case DMU_BITANGENT_Z: {
        float z = bitangent().z;
        DMU_GetValue(DMT_SURFACE_BITANGENT, &z, &args, 0);
        break; }

    case DMU_BITANGENT_XYZ: {
        Vector3f const &btn = bitangent();
        DMU_GetValue(DMT_SURFACE_BITANGENT, &btn.x, &args, 0);
        DMU_GetValue(DMT_SURFACE_BITANGENT, &btn.y, &args, 1);
        DMU_GetValue(DMT_SURFACE_BITANGENT, &btn.z, &args, 2);
        break; }

    case DMU_NORMAL_X: {
        float x = normal().x;
        DMU_GetValue(DMT_SURFACE_NORMAL, &x, &args, 0);
        break; }

    case DMU_NORMAL_Y: {
        float y = normal().y;
        DMU_GetValue(DMT_SURFACE_NORMAL, &y, &args, 0);
        break; }

    case DMU_NORMAL_Z: {
        float z = normal().z;
        DMU_GetValue(DMT_SURFACE_NORMAL, &z, &args, 0);
        break; }

    case DMU_NORMAL_XYZ: {
        Vector3f const &nrm = normal();
        DMU_GetValue(DMT_SURFACE_NORMAL, &nrm.x, &args, 0);
        DMU_GetValue(DMT_SURFACE_NORMAL, &nrm.y, &args, 1);
        DMU_GetValue(DMT_SURFACE_NORMAL, &nrm.z, &args, 2);
        break; }

    case DMU_COLOR:
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor.x, &args, 0);
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor.y, &args, 1);
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor.z, &args, 2);
        DMU_GetValue(DMT_SURFACE_RGBA, &d->opacity, &args, 2);
        break;

    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor.x, &args, 0);
        break;

    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor.y, &args, 0);
        break;

    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SURFACE_RGBA, &d->tintColor.z, &args, 0);
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
        return MapElement::property(args);
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
        float red, green, blue;
        DMU_SetValue(DMT_SURFACE_RGBA, &red,   &args, 0);
        DMU_SetValue(DMT_SURFACE_RGBA, &green, &args, 1);
        DMU_SetValue(DMT_SURFACE_RGBA, &blue,  &args, 2);
        setTintColor(red, green, blue);
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
        Vector2f newOrigin = d->materialOrigin;
        DMU_SetValue(DMT_SURFACE_OFFSET, &newOrigin.x, &args, 0);
        DMU_SetValue(DMT_SURFACE_OFFSET, &newOrigin.y, &args, 1);
        setMaterialOrigin(newOrigin);
        break; }

    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}
