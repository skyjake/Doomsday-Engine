/** @file surface.cpp  World map surface.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "world/surface.h"

#ifdef __CLIENT__
#  include <QList>
#endif
#include <QtAlgorithms>
#include <de/vector1.h>
#include <de/Log>
#include "de_defs.h" // Def_GetGenerator
#include "dd_loop.h" // frameTimePos

#include "MaterialManifest"
#include "TextureManifest"

#include "world/map.h"
#include "Plane"
#include "world/worldsystem.h" // ddMapSetup

#ifdef __CLIENT__
#  include "gl/gl_tex.h"

#  include "Decoration"

#  include "render/rend_main.h"
#endif

using namespace de;

DENG2_PIMPL(Surface)
{
    int flags = 0;                              ///< @ref sufFlags

    Matrix3f tangentMatrix { Matrix3f::Zero };  ///< Tangent space vectors.
    bool needUpdateTangentMatrix = false;       ///< @c true= marked for update.

    Material *material = nullptr;               ///< Currently bound material.
    bool materialIsMissingFix = false;          ///< @c true= @ref material is a "missing fix".
    Vector2f materialOrigin;                    ///< @em sharp surface space material origin.

    Vector3f tintColor;
    float opacity = 0;
    blendmode_t blendMode { BM_NORMAL };

#ifdef __CLIENT__
    typedef QList<Decoration *> Decorations;
    Decorations decorations;                    ///< Surface (light) decorations (owned).
    bool needDecorationUpdate = true;           ///< @c true= An update is needed.

    Vector2f oldMaterialOrigin[2];              ///< Old @em sharp surface space material origins, for smoothing.
    Vector2f materialOriginSmoothed;            ///< @em smoothed surface space material origin.
    Vector2f materialOriginSmoothedDelta;       ///< Delta between @em sharp and @em smoothed.
#endif

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
#ifdef __CLIENT__
        // Stop scroll interpolation for this surface.
        map().scrollingSurfaces().remove(&self);

        // Stop material redecoration for this surface.
        map().unlinkInMaterialLists(&self);

        qDeleteAll(decorations);
#endif
    }

    inline Map &map() const { return self.map(); }
    inline MapElement &parent() const { return self.parent(); }

#ifdef DENG_DEBUG
    inline bool isSideMiddle() const
    {
        return parent().type() == DMU_SIDE &&
               &self == &parent().as<LineSide>().middle();
    }

    inline bool isSectorExtraPlane() const
    {
        if(parent().type() != DMU_PLANE) return false;
        auto const &plane = parent().as<Plane>();
        return !(plane.isSectorFloor() || plane.isSectorCeiling());
    }
#endif

    void updateTangentMatrix()
    {
        needUpdateTangentMatrix = false;

        dfloat values[9];
        Vector3f normal = tangentMatrix.column(2);
        V3f_Set(values + 6, normal.x, normal.y, normal.z);
        V3f_BuildTangents(values, values + 3, values + 6);

        tangentMatrix = Matrix3f(values);
    }

    void notifyMaterialOriginChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(MaterialOriginChange, i) i->surfaceMaterialOriginChanged(self);

#ifdef __CLIENT__
        if(!ddMapSetup)
        {
            needDecorationUpdate = true;
            map().scrollingSurfaces().insert(&self);
        }
#endif
    }

    void notifyNormalChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(NormalChange, i) i->surfaceNormalChanged(self);
    }

    void notifyOpacityChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(OpacityChange, i) i->surfaceOpacityChanged(self);
    }

    void notifyTintColorChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(TintColorChange, i) i->surfaceTintColorChanged(self);
    }

    DENG2_PIMPL_AUDIENCE(MaterialOriginChange)
    DENG2_PIMPL_AUDIENCE(NormalChange)
    DENG2_PIMPL_AUDIENCE(OpacityChange)
    DENG2_PIMPL_AUDIENCE(TintColorChange)
};

DENG2_AUDIENCE_METHOD(Surface, MaterialOriginChange)
DENG2_AUDIENCE_METHOD(Surface, NormalChange)
DENG2_AUDIENCE_METHOD(Surface, OpacityChange)
DENG2_AUDIENCE_METHOD(Surface, TintColorChange)

Surface::Surface(MapElement &owner, float opacity, Vector3f const &tintColor)
    : MapElement(DMU_SURFACE, &owner)
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
    Vector3f const oldNormal = normal();
    Vector3f const newNormalNormalized = newNormal.normalize();
    if(oldNormal != newNormalNormalized)
    {
        for(int i = 0; i < 3; ++i)
        {
            d->tangentMatrix.at(i, 2) = newNormalNormalized[i];
        }

        // We'll need to recalculate the tangents when next referenced.
        d->needUpdateTangentMatrix = true;
        d->notifyNormalChanged();
    }
    return *this;
}

bool Surface::hasMaterial() const
{
    return d->material != nullptr;
}

bool Surface::hasFixMaterial() const
{
    return hasMaterial() && d->materialIsMissingFix;
}

Material &Surface::material() const
{
    if(d->material) return *d->material;
    /// @throw MissingMaterialError Attempted with no material bound.
    throw MissingMaterialError("Surface::material", "No material is bound");
}

Material *Surface::materialPtr() const
{
    return hasMaterial()? &material() : nullptr;
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
    d->needDecorationUpdate = true;

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
                parent().as<Plane>().spawnParticleGen(def);
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
            d->materialOriginSmoothed      = d->materialOrigin;
            d->materialOriginSmoothedDelta = Vector2f();

            d->oldMaterialOrigin[0] = d->oldMaterialOrigin[1] = d->materialOrigin;
        }
#endif

        d->notifyMaterialOriginChanged();
    }
    return *this;
}

bool Surface::materialMirrorX() const
{
    return (d->flags & DDSUF_MATERIAL_FLIPH) != 0;
}

bool Surface::materialMirrorY() const
{
    return (d->flags & DDSUF_MATERIAL_FLIPV) != 0;
}

Vector2f Surface::materialScale() const
{
    return Vector2f(materialMirrorX()? -1 : 1, materialMirrorY()? -1 : 1);
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

Vector2f const &Surface::materialOriginSmoothedAsDelta() const
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
    markForDecorationUpdate();
#endif
}

void Surface::resetSmoothedMaterialOrigin()
{
    // $smoothmaterialorigin
    d->materialOriginSmoothed = d->oldMaterialOrigin[0] = d->oldMaterialOrigin[1] = d->materialOrigin;
    d->materialOriginSmoothedDelta = Vector2f();

#ifdef __CLIENT__
    markForDecorationUpdate();
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
    Vector3f const newColorClamped(de::clamp(0.f, newTintColor.x, 1.f),
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
    if(!d->material || d->material->isSkyMasked())
    {
        color = Vector3f();
        return 0;
    }

    MaterialAnimator &matAnimator = d->material->getAnimator(Rend_MapSurfaceMaterialSpec());

    // Ensure we've up to date info about the material.
    matAnimator.prepare();

    TextureVariant *texture       = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
    auto const *avgColorAmplified = reinterpret_cast<averagecolor_analysis_t const *>(texture->base().analysisDataPointer(Texture::AverageColorAmplifiedAnalysis));
    if(!avgColorAmplified) throw Error("Surface::glow", "Texture \"" + texture->base().manifest().composeUri().asText() + "\" has no AverageColorAmplifiedAnalysis");

    color = Vector3f(avgColorAmplified->color.rgb);
    return matAnimator.glowStrength() * glowFactor; // Global scale factor.
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
    qDeleteAll(d->decorations); d->decorations.clear();
}

LoopResult Surface::forAllDecorations(std::function<LoopResult (Decoration &)> func) const
{
    for(Decoration *decor : d->decorations)
    {
        if(auto result = func(*decor)) return result;
    }
    return LoopContinue;
}

int Surface::decorationCount() const
{
    return d->decorations.count();
}

void Surface::markForDecorationUpdate(bool yes)
{
    if(ddMapSetup) return;
    d->needDecorationUpdate = yes;
}

bool Surface::needsDecorationUpdate() const
{
    return d->needDecorationUpdate;
}

#endif // __CLIENT__

int Surface::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_MATERIAL: {
        Material *mat = (d->materialIsMissingFix? nullptr : d->material);
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
