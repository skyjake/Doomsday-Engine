/** @file surface.cpp  World map surface.
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

#include "de_base.h"
#include "world/surface.h"

#include "de_defs.h" // Def_GetGenerator
#include "dd_loop.h" // frameTimePos

#include "MaterialManifest"
#include "TextureManifest"

#include "world/map.h"
#include "Plane"
#include "world/world.h" // ddMapSetup

#ifdef __CLIENT__
#  include "gl/gl_tex.h"

#  include "MaterialSnapshot"
#  include "Decoration"

#  include "render/rend_main.h"
#endif

#include <de/Log>
#include <de/vector1.h>
#include <QtAlgorithms>

using namespace de;

DENG2_PIMPL(Surface)
{
    Matrix3f tangentMatrix;        ///< Tangent space vectors.
    bool needUpdateTangentMatrix;  ///< @c true= marked for update.
    Material *material;            ///< Currently bound material.
    bool materialIsMissingFix;     ///< @c true= @ref material is a "missing fix".
    Vector2f materialOrigin;       ///< @em sharp surface space material origin.
    Vector3f tintColor;
    float opacity;
    blendmode_t blendMode;
    int flags;                     ///< @ref sufFlags

#ifdef __CLIENT__
    Decorations decorations;       ///< Surface (light) decorations (owned).

    Vector2f oldMaterialOrigin[2];        ///< Old @em sharp surface space material origins, for smoothing.
    Vector2f materialOriginSmoothed;      ///< @em smoothed surface space material origin.
    Vector2f materialOriginSmoothedDelta; ///< Delta between @em sharp and @em smoothed.
#endif

    Instance(Public *i)
        : Base(i)
        , tangentMatrix(Matrix3f::Zero)
        , needUpdateTangentMatrix(false)
        , material(0)
        , materialIsMissingFix(false)
        , opacity(0)
        , blendMode(BM_NORMAL)
        , flags(0)
    {}

    ~Instance()
    {
#ifdef __CLIENT__
        // Stop scroll interpolation for this surface.
        self.map().scrollingSurfaces().remove(&self);

        // Stop material redecoration for this surface.
        self.map().unlinkInMaterialLists(&self);

        qDeleteAll(decorations);
#endif
    }

#ifdef DENG_DEBUG
    inline bool isSideMiddle()
    {
        return self.parent().type() == DMU_SIDE &&
               &self == &self.parent().as<LineSide>().middle();
    }

    inline bool isSectorExtraPlane()
    {
        if(self.parent().type() != DMU_PLANE) return false;
        Plane const &plane = self.parent().as<Plane>();
        return !(plane.isSectorFloor() || plane.isSectorCeiling());
    }
#endif

    void notifyNormalChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(NormalChange, i)
        {
            i->surfaceNormalChanged(self);
        }
    }

    void notifyMaterialOriginChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(MaterialOriginChange, i)
        {
            i->surfaceMaterialOriginChanged(self);
        }

#ifdef __CLIENT__
        if(!ddMapSetup)
        {
            self._needDecorationUpdate = true;

            self.map().scrollingSurfaces().insert(&self);
        }
#endif
    }

    void notifyOpacityChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OpacityChange, i)
        {
            i->surfaceOpacityChanged(self);
        }
    }

    void notifyTintColorChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(TintColorChange, i)
        {
            i->surfaceTintColorChanged(self);
        }
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
    : MapElement(DMU_SURFACE, &owner)
#ifdef __CLIENT__
    , _needDecorationUpdate(true)
#endif
    , d(new Instance(this))
{
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

Surface &Surface::setNormal(Vector3f const &newNormal)
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

        d->notifyNormalChanged();
    }
    return *this;
}

int Surface::flags() const
{
    return d->flags;
}

Surface &Surface::setFlags(int flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
    return *this;
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

Surface &Surface::setMaterial(Material *newMaterial, bool isMissingFix)
{
    if(d->material == newMaterial)
        return *this;

    // Update the missing-material-fix state.
    if(!d->material)
    {
        if(newMaterial && isMissingFix)
        {
            d->materialIsMissingFix = true;

            // Sides of selfreferencing map lines should never receive fix materials.
            DENG2_ASSERT(!(parent().type() == DMU_SIDE && parent().as<LineSide>().line().isSelfReferencing()));
        }
    }
    else if(newMaterial && d->materialIsMissingFix)
    {
        d->materialIsMissingFix = false;
    }

    d->material = newMaterial;

#ifdef __CLIENT__
    // When the material changes any existing decorations are cleared.
    clearDecorations();
    _needDecorationUpdate = true;

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
                P_SpawnPlaneParticleGen(def, &parent().as<Plane>());
            }

        }
    }
#endif // __CLIENT__

    return *this;
}

Vector2f const &Surface::materialOrigin() const
{
    return d->materialOrigin;
}

Surface &Surface::setMaterialOrigin(Vector2f const &newOrigin)
{
    if(d->materialOrigin != newOrigin)
    {
        d->materialOrigin = newOrigin;

#ifdef __CLIENT__
        // During map setup we'll apply this immediately to the visual origin also.
        if(ddMapSetup)
        {
            d->materialOriginSmoothed = d->materialOrigin;
            d->materialOriginSmoothedDelta.x = d->materialOriginSmoothedDelta.y = 0;

            d->oldMaterialOrigin[0] = d->oldMaterialOrigin[1] = d->materialOrigin;
        }
#endif

        d->notifyMaterialOriginChanged();
    }
    return *this;
}

de::Uri Surface::composeMaterialUri() const
{
    if(!hasMaterial()) return de::Uri();
    return material().manifest().composeUri();
}

#ifdef __CLIENT__

Vector2f const &Surface::materialOriginSmoothed() const
{
    return d->materialOriginSmoothed;
}

Vector2f const &Surface::materialOriginSmoothedDelta() const
{
    return d->materialOriginSmoothedDelta;
}

void Surface::lerpSmoothedMaterialOrigin()
{
    // $smoothmaterialorigin
    d->materialOriginSmoothedDelta = d->oldMaterialOrigin[0] * (1 - frameTimePos)
        + d->materialOrigin * frameTimePos - d->materialOrigin;

    // Visible material origin.
    d->materialOriginSmoothed = d->materialOrigin + d->materialOriginSmoothedDelta;

#ifdef __CLIENT__
    markAsNeedingDecorationUpdate();
#endif
}

void Surface::resetSmoothedMaterialOrigin()
{
    // $smoothmaterialorigin
    d->materialOriginSmoothed = d->oldMaterialOrigin[0] = d->oldMaterialOrigin[1] = d->materialOrigin;
    d->materialOriginSmoothedDelta.x = d->materialOriginSmoothedDelta.y = 0;

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

Surface &Surface::setOpacity(float newOpacity)
{
    DENG2_ASSERT(d->isSideMiddle() || d->isSectorExtraPlane()); // sanity check

    newOpacity = de::clamp(0.f, newOpacity, 1.f);
    if(!de::fequal(d->opacity, newOpacity))
    {
        d->opacity = newOpacity;
        d->notifyOpacityChanged();
    }
    return *this;
}

Vector3f const &Surface::tintColor() const
{
    return d->tintColor;
}

Surface &Surface::setTintColor(Vector3f const &newTintColor)
{
    Vector3f newColorClamped(de::clamp(0.f, newTintColor.x, 1.f),
                             de::clamp(0.f, newTintColor.y, 1.f),
                             de::clamp(0.f, newTintColor.z, 1.f));

    if(d->tintColor != newColorClamped)
    {
        d->tintColor = newColorClamped;
        d->notifyTintColorChanged();
    }
    return *this;
}

blendmode_t Surface::blendMode() const
{
    return d->blendMode;
}

Surface &Surface::setBlendMode(blendmode_t newBlendMode)
{
    d->blendMode = newBlendMode;
    return *this;
}

#ifdef __CLIENT__
float Surface::glow(Vector3f &color) const
{
    if(!d->material || !d->material->hasGlow())
    {
        color = Vector3f();
        return 0;
    }

    MaterialSnapshot const &ms = d->material->prepare(Rend_MapSurfaceMaterialSpec());
    averagecolor_analysis_t const *avgColorAmplified = reinterpret_cast<averagecolor_analysis_t const *>(ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer(Texture::AverageColorAmplifiedAnalysis));
    if(!avgColorAmplified) throw Error("Surface::glow", QString("Texture \"%1\" has no AverageColorAmplifiedAnalysis").arg(ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri()));

    color = Vector3f(avgColorAmplified->color.rgb);
    return ms.glowStrength() * glowFactor; // Global scale factor.
}

void Surface::addDecoration(Decoration *decoration)
{
    if(!decoration) return;
    d->decorations.append(decoration);

    decoration->setSurface(this);
    if(hasMap()) decoration->setMap(&map());
}

void Surface::clearDecorations()
{
    qDeleteAll(d->decorations);
    d->decorations.clear();
}

Surface::Decorations const &Surface::decorations() const
{
    return d->decorations;
}

int Surface::decorationCount() const
{
    return d->decorations.count();
}

void Surface::markAsNeedingDecorationUpdate()
{
    if(ddMapSetup) return;

    _needDecorationUpdate = true;
}
#endif // __CLIENT__

int Surface::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_MATERIAL: {
        Material *mat = d->materialIsMissingFix? 0 : d->material;
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
        Vector3f newColor = d->tintColor;
        args.value(DMT_SURFACE_RGBA, &newColor.x, 0);
        args.value(DMT_SURFACE_RGBA, &newColor.y, 1);
        args.value(DMT_SURFACE_RGBA, &newColor.z, 2);
        setTintColor(newColor);
        break; }

    case DMU_COLOR_RED: {
        Vector3f newColor = d->tintColor;
        args.value(DMT_SURFACE_RGBA, &newColor.x, 0);
        setTintColor(newColor);
        break; }

    case DMU_COLOR_GREEN: {
        Vector3f newColor = d->tintColor;
        args.value(DMT_SURFACE_RGBA, &newColor.y, 0);
        setTintColor(newColor);
        break; }

    case DMU_COLOR_BLUE: {
        Vector3f newColor = d->tintColor;
        args.value(DMT_SURFACE_RGBA, &newColor.z, 0);
        setTintColor(newColor);
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
        Vector2f newOrigin = d->materialOrigin;
        args.value(DMT_SURFACE_OFFSET, &newOrigin.x, 0);
        setMaterialOrigin(newOrigin);
        break; }

    case DMU_OFFSET_Y: {
        Vector2f newOrigin = d->materialOrigin;
        args.value(DMT_SURFACE_OFFSET, &newOrigin.y, 0);
        setMaterialOrigin(newOrigin);
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
