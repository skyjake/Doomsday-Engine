/** @file surface.cpp World map surface.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "dd_main.h"

#include "Materials"

#include "world/map.h"
#include "world/world.h" // ddMapSetup

#include "render/r_main.h" // frameTimePos

#include "world/surface.h"

using namespace de;

DENG2_PIMPL(Surface)
{
    /// Tangent space matrix.
    Matrix3f tangentMatrix;
    bool needUpdateTangentMatrix; ///< @c true= marked for update.

    /// Bound material.
    Material *material;

    /// @c true= Bound material is a "missing material fix".
    bool materialIsMissingFix;

    /// @em Sharp origin of the surface material.
    Vector2f materialOrigin;

#ifdef __CLIENT__

    /// Old @em sharp origin of the surface material, for smoothing.
    Vector2f oldMaterialOrigin[2];

    /// Smoothed origin of the surface material.
    Vector2f visMaterialOrigin;

    /// Delta between the @em sharp and smoothed origin of the surface material.
    Vector2f visMaterialOriginDelta;

#endif

    /// Surface color tint.
    de::Vector3f tintColor;

    /// Surface opacity.
    float opacity;

    /// Blending mode.
    blendmode_t blendMode;

    /// @ref sufFlags
    int flags;

    Instance(Public *i)
        : Base(i),
          tangentMatrix(Matrix3f::Zero),
          needUpdateTangentMatrix(false),
          material(0),
          materialIsMissingFix(false),
          blendMode(BM_NORMAL),
          flags(0)
    {}

#ifdef DENG_DEBUG
    inline bool isSideMiddle()
    {
        return self.parent().type() == DMU_SIDE &&
               &self == &self.parent().as<Line::Side>()->middle();
    }

    inline bool isSectorExtraPlane()
    {
        if(self.parent().type() != DMU_PLANE) return false;
        Plane const &plane = *self.parent().as<Plane>();
        return !(plane.isSectorFloor() || plane.isSectorCeiling());
    }
#endif

    void notifyNormalChanged(Vector3f const &oldNormal)
    {
        // Predetermine which axes have changed.
        int changedAxes = 0;
        for(int i = 0; i < 3; ++i)
        {
            if(!de::fequal(tangentMatrix.at(i, 2), oldNormal[i]))
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

#ifdef __CLIENT__
        if(!ddMapSetup)
        {
            /// @todo Replace with a de::Observer-based mechanism.
            self._decorationData.needsUpdate = true;

            self.map().scrollingSurfaces().insert(&self);
        }
#endif
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

    void updateTangentMatrix()
    {
        needUpdateTangentMatrix = false;

        dfloat values[9];
        Vector3f normal = tangentMatrix.column(2);
        V3f_Set(values + 6, normal.x, normal.y, normal.z);
        V3f_BuildTangents(values, values + 3, values + 6);

        tangentMatrix = Matrix3f(values);
    }
};

Surface::Surface(MapElement &owner, float opacity, Vector3f const &tintColor)
    : MapElement(DMU_SURFACE, &owner),
      d(new Instance(this))
{
#ifdef __CLIENT__
    _decorationData.needsUpdate = true;
    _decorationData.sources     = 0;
    _decorationData.numSources  = 0;
#endif

    d->opacity   = opacity;
    d->tintColor = tintColor;
}

Matrix3f const &Surface::tangentMatrix() const
{
    // Perform any scheduled update now.
    if(d->needUpdateTangentMatrix)
    {
        d->updateTangentMatrix();
    }
    return d->tangentMatrix;
}

Vector3f Surface::tangent() const
{
    return tangentMatrix().column(0);
}

Vector3f Surface::bitangent() const
{
    return tangentMatrix().column(1);
}

Vector3f Surface::normal() const
{
    return tangentMatrix().column(2);
}

void Surface::setNormal(Vector3f const &newNormal)
{
    Vector3f oldNormal = normal();
    Vector3f newNormalNormalized = newNormal.normalize();
    if(oldNormal != newNormalNormalized)
    {
        d->tangentMatrix.at(0, 2) = newNormalNormalized.x;
        d->tangentMatrix.at(1, 2) = newNormalNormalized.y;
        d->tangentMatrix.at(2, 2) = newNormalNormalized.z;

        // We'll need to recalculate the tangents when next referenced.
        d->needUpdateTangentMatrix = true;

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

void Surface::setFlags(int flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
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
                DENG_ASSERT(!(parent().type() == DMU_SIDE && parent().as<Line::Side>()->line().isSelfReferencing()));
            }
        }
        else if(newMaterial && d->materialIsMissingFix)
        {
            d->materialIsMissingFix = false;
        }

        d->material = newMaterial;

#ifdef __CLIENT__
        if(!ddMapSetup)
        {
            map().unlinkInMaterialLists(this);

            if(d->material)
            {
                map().linkInMaterialLists(this);

                if(parent().type() == DMU_PLANE)
                {
                    de::Uri uri = d->material->manifest().composeUri();
                    ded_ptcgen_t const *def = Def_GetGenerator(reinterpret_cast<uri_s *>(&uri));
                    P_SpawnPlaneParticleGen(def, parent().as<Plane>());
                }

            }
        }

        /// @todo Replace with a de::Observer-based mechanism.
        _decorationData.needsUpdate = true;
#endif // __CLIENT__
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

#ifdef __CLIENT__
        // During map setup we'll apply this immediately to the visual origin also.
        if(ddMapSetup)
        {
            d->visMaterialOrigin = d->materialOrigin;
            d->visMaterialOriginDelta.x = d->visMaterialOriginDelta.y = 0;

            d->oldMaterialOrigin[0] = d->oldMaterialOrigin[1] = d->materialOrigin;
        }
#endif

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

#ifdef __CLIENT__
        // During map setup we'll apply this immediately to the visual origin also.
        if(ddMapSetup)
        {
            d->visMaterialOrigin[component] = d->materialOrigin[component];
            d->visMaterialOriginDelta[component] = 0;

            d->oldMaterialOrigin[0][component] =
                d->oldMaterialOrigin[1][component] =
                    d->materialOrigin[component];
        }
#endif

        // Notify interested parties of the change.
        d->notifyMaterialOriginChanged(oldMaterialOrigin, (1 << component));
    }
}

de::Uri Surface::composeMaterialUri() const
{
    if(!hasMaterial()) return de::Uri();
    return material().manifest().composeUri();
}

#ifdef __CLIENT__

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

#endif // __CLIENT__

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

void Surface::setTintColor(Vector3f const &newTintColor)
{
    Vector3f newColorClamped = Vector3f(de::clamp(0.f, newTintColor.x, 1.f),
                                        de::clamp(0.f, newTintColor.y, 1.f),
                                        de::clamp(0.f, newTintColor.z, 1.f));
    if(d->tintColor != newColorClamped)
    {
        Vector3f oldTintColor = d->tintColor;
        d->tintColor = newColorClamped;

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
        for(int i = 0; i < _decorationData.numSources - 1; ++i)
        {
            Surface::DecorSource *d = &newSources[i];
            Surface::DecorSource *s = &_decorationData.sources[i];

            std::memcpy(d, s, sizeof(*d));
        }

        Z_Free(_decorationData.sources);
    }

    Surface::DecorSource *d = &newSources[_decorationData.numSources - 1];
    _decorationData.sources = newSources;

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

int Surface::decorationCount() const
{
    return _decorationData.numSources;
}

void Surface::markAsNeedingDecorationUpdate()
{
    if(ddMapSetup) return;

    _decorationData.needsUpdate = true;
}
#endif // __CLIENT__

int Surface::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_MATERIAL: {
        Material *mat = d->material;
        if(d->materialIsMissingFix)
            mat = 0;
        args.setValue(DMT_SURFACE_MATERIAL, &mat, 0);
        break; }

    case DMU_OFFSET_X:
        args.setValue(DMT_SURFACE_OFFSET, &d->materialOrigin.x, 0);
        break;

    case DMU_OFFSET_Y:
        args.setValue(DMT_SURFACE_OFFSET, &d->materialOrigin.y, 0);
        break;

    case DMU_OFFSET_XY:
        args.setValue(DMT_SURFACE_OFFSET, &d->materialOrigin.x, 0);
        args.setValue(DMT_SURFACE_OFFSET, &d->materialOrigin.y, 1);
        break;

    case DMU_TANGENT_X:
        args.setValue(DMT_SURFACE_TANGENT, &d->tangentMatrix.at(0, 0), 0);
        break;

    case DMU_TANGENT_Y:
        args.setValue(DMT_SURFACE_TANGENT, &d->tangentMatrix.at(1, 0), 0);
        break;

    case DMU_TANGENT_Z:
        args.setValue(DMT_SURFACE_TANGENT, &d->tangentMatrix.at(2, 0), 0);
        break;

    case DMU_TANGENT_XYZ:
        args.setValue(DMT_SURFACE_TANGENT, &d->tangentMatrix.at(0, 0), 0);
        args.setValue(DMT_SURFACE_TANGENT, &d->tangentMatrix.at(1, 0), 1);
        args.setValue(DMT_SURFACE_TANGENT, &d->tangentMatrix.at(2, 0), 2);
        break;

    case DMU_BITANGENT_X:
        args.setValue(DMT_SURFACE_BITANGENT, &d->tangentMatrix.at(0, 1), 0);
        break;

    case DMU_BITANGENT_Y:
        args.setValue(DMT_SURFACE_BITANGENT, &d->tangentMatrix.at(1, 1), 0);
        break;

    case DMU_BITANGENT_Z:
        args.setValue(DMT_SURFACE_BITANGENT, &d->tangentMatrix.at(2, 1), 0);
        break;

    case DMU_BITANGENT_XYZ:
        args.setValue(DMT_SURFACE_BITANGENT, &d->tangentMatrix.at(0, 1), 0);
        args.setValue(DMT_SURFACE_BITANGENT, &d->tangentMatrix.at(1, 1), 1);
        args.setValue(DMT_SURFACE_BITANGENT, &d->tangentMatrix.at(2, 1), 2);
        break;

    case DMU_NORMAL_X:
        args.setValue(DMT_SURFACE_NORMAL, &d->tangentMatrix.at(0, 2), 0);
        break;

    case DMU_NORMAL_Y:
        args.setValue(DMT_SURFACE_NORMAL, &d->tangentMatrix.at(1, 2), 0);
        break;

    case DMU_NORMAL_Z:
        args.setValue(DMT_SURFACE_NORMAL, &d->tangentMatrix.at(2, 2), 0);
        break;

    case DMU_NORMAL_XYZ:
        args.setValue(DMT_SURFACE_NORMAL, &d->tangentMatrix.at(0, 2), 0);
        args.setValue(DMT_SURFACE_NORMAL, &d->tangentMatrix.at(1, 2), 1);
        args.setValue(DMT_SURFACE_NORMAL, &d->tangentMatrix.at(2, 2), 2);
        break;

    case DMU_COLOR:
        args.setValue(DMT_SURFACE_RGBA, &d->tintColor.x, 0);
        args.setValue(DMT_SURFACE_RGBA, &d->tintColor.y, 1);
        args.setValue(DMT_SURFACE_RGBA, &d->tintColor.z, 2);
        args.setValue(DMT_SURFACE_RGBA, &d->opacity, 2);
        break;

    case DMU_COLOR_RED:
        args.setValue(DMT_SURFACE_RGBA, &d->tintColor.x, 0);
        break;

    case DMU_COLOR_GREEN:
        args.setValue(DMT_SURFACE_RGBA, &d->tintColor.y, 0);
        break;

    case DMU_COLOR_BLUE:
        args.setValue(DMT_SURFACE_RGBA, &d->tintColor.z, 0);
        break;

    case DMU_ALPHA:
        args.setValue(DMT_SURFACE_RGBA, &d->opacity, 0);
        break;

    case DMU_BLENDMODE:
        args.setValue(DMT_SURFACE_BLENDMODE, &d->blendMode, 0);
        break;

    case DMU_FLAGS:
        args.setValue(DMT_SURFACE_FLAGS, &d->flags, 0);
        break;

    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

int Surface::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_BLENDMODE: {
        blendmode_t newBlendMode;
        args.value(DMT_SURFACE_BLENDMODE, &newBlendMode, 0);
        setBlendMode(newBlendMode);
        break; }

    case DMU_FLAGS:
        args.value(DMT_SURFACE_FLAGS, &d->flags, 0);
        break;

    case DMU_COLOR: {
        Vector3f newColor;
        args.value(DMT_SURFACE_RGBA, &newColor.x, 0);
        args.value(DMT_SURFACE_RGBA, &newColor.y, 1);
        args.value(DMT_SURFACE_RGBA, &newColor.z, 2);
        setTintColor(newColor);
        break; }

    case DMU_COLOR_RED: {
        float newRed;
        args.value(DMT_SURFACE_RGBA, &newRed, 0);
        setTintRed(newRed);
        break; }

    case DMU_COLOR_GREEN: {
        float newGreen;
        args.value(DMT_SURFACE_RGBA, &newGreen, 0);
        setTintGreen(newGreen);
        break; }

    case DMU_COLOR_BLUE: {
        float newBlue;
        args.value(DMT_SURFACE_RGBA, &newBlue, 0);
        setTintBlue(newBlue);
        break; }

    case DMU_ALPHA: {
        float newOpacity;
        args.value(DMT_SURFACE_RGBA, &newOpacity, 0);
        setOpacity(newOpacity);
        break; }

    case DMU_MATERIAL: {
        Material *newMaterial;
        args.value(DMT_SURFACE_MATERIAL, &newMaterial, 0);
        setMaterial(newMaterial);
        break; }

    case DMU_OFFSET_X: {
        float newOriginX;
        args.value(DMT_SURFACE_OFFSET, &newOriginX, 0);
        setMaterialOriginX(newOriginX);
        break; }

    case DMU_OFFSET_Y: {
        float newOriginY;
        args.value(DMT_SURFACE_OFFSET, &newOriginY, 0);
        setMaterialOriginY(newOriginY);
        break; }

    case DMU_OFFSET_XY: {
        Vector2f newOrigin = d->materialOrigin;
        args.value(DMT_SURFACE_OFFSET, &newOrigin.x, 0);
        args.value(DMT_SURFACE_OFFSET, &newOrigin.y, 1);
        setMaterialOrigin(newOrigin);
        break; }

    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}
