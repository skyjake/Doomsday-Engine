/** @file surface.cpp  World map surface.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/surface.h"

#include "doomsday/world/map.h"
#include "doomsday/world/plane.h"
#include "doomsday/world/line.h"
#include "doomsday/world/world.h"
#include "doomsday/world/materialmanifest.h"
#include "doomsday/world/material.h"

//#include "misc/r_util.h" // R_NameForBlendMode()

#include <de/log.h>
#include <de/legacy/vector1.h>

namespace world {

using namespace de;

#ifdef DE_DEBUG
static inline bool Surface_isSideMiddle(const Surface &suf)
{
    return suf.parent().type() == DMU_SIDE
           && &suf == &suf.parent().as<LineSide>().middle();
}

static inline bool Surface_isSectorExtraPlane(const Surface &suf)
{
    if (suf.parent().type() != DMU_PLANE) return false;
    const auto &plane = suf.parent().as<Plane>();
    return !(plane.isSectorFloor() || plane.isSectorCeiling());
}
#endif

DE_PIMPL(Surface)
{
    dint flags = 0;                         ///< @ref sufFlags

    Mat3f tangentMatrix { Mat3f::Zero };    ///< Tangent space vectors.
    bool needUpdateTangentMatrix = false;   ///< @c true= marked for update.

    Material *material = nullptr;           ///< Currently bound material.
    bool materialIsMissingFix = false;      ///< @c true= @ref material is a "missing fix".

    Vec2f origin;                           ///< @em sharp offset to surface-space origin.
    Vec3f color;
    float opacity = 0;
    blendmode_t blendMode { BM_NORMAL };

    Impl(Public *i) : Base(i)
    {}

    inline MapElement &owner() const { return self().parent(); }

#ifdef DE_DEBUG
    bool isSideMiddle() const
    {
        return owner().type() == DMU_SIDE
               && thisPublic == &owner().as<LineSide>().middle();
    }

    bool isSectorExtraPlane() const
    {
        if (owner().type() != DMU_PLANE) return false;
        const auto &plane = owner().as<Plane>();
        return !(plane.isSectorFloor() || plane.isSectorCeiling());
    }
#endif

    void updateTangentMatrix()
    {
        needUpdateTangentMatrix = false;

        float values[9];
        Vec3f normal = tangentMatrix.column(2);
        V3f_Set(values + 6, normal.x, normal.y, normal.z);
        V3f_BuildTangents(values, values + 3, values + 6);

        tangentMatrix = Mat3f(values);
    }

    DE_PIMPL_AUDIENCES(ColorChange, MaterialChange, NormalChange, OpacityChange, OriginChange)
};

DE_AUDIENCE_METHODS(Surface, ColorChange, MaterialChange, NormalChange, OpacityChange, OriginChange)

Surface::Surface(MapElement &owner, float opacity, const Vec3f &color)
    : MapElement(DMU_SURFACE, &owner)
    , d(new Impl(this))
{
    d->color   = color;
    d->opacity = opacity;
}

String Surface::description() const
{
    auto desc = Stringf(
                           _E(l) "Material: "        _E(.)_E(i) "%s" _E(.)
                       " " _E(l) "Material Origin: " _E(.)_E(i) "%s" _E(.)
                       " " _E(l) "Normal: "          _E(.)_E(i) "%s" _E(.)
                       " " _E(l) "Opacity: "         _E(.)_E(i) "%f" _E(.)
                       " " _E(l) "Blend Mode: "      _E(.)_E(i) "%s" _E(.)
                       " " _E(l) "Tint Color: "      _E(.)_E(i) "%s" _E(.),
                  hasMaterial() ? material().manifest().composeUri().asText().c_str() : "None",
                  origin().asText().c_str(),
                  normal().asText().c_str(),
                  opacity(),
                  DGL_NameForBlendMode(blendMode()),
                  color().asText().c_str());

#ifdef DE_DEBUG
    desc.prepend(Stringf(_E(b) "Surface " _E(.) "[%p]\n", this));
#endif
    return desc;
}

const Mat3f &Surface::tangentMatrix() const
{
    // Perform any scheduled update now.
    if (d->needUpdateTangentMatrix)
    {
        d->updateTangentMatrix();
    }
    return d->tangentMatrix;
}

Surface &Surface::setNormal(const Vec3f &newNormal)
{
    const Vec3f oldNormal = normal();
    const Vec3f newNormalNormalized = newNormal.normalize();
    if (oldNormal != newNormalNormalized)
    {
        for (dint i = 0; i < 3; ++i)
        {
            d->tangentMatrix.at(i, 2) = newNormalNormalized[i];
        }

        // We'll need to recalculate the tangents when next referenced.
        d->needUpdateTangentMatrix = true;
        DE_NOTIFY(NormalChange, i) i->surfaceNormalChanged(*this);
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
    if (d->material) return *d->material;
    /// @throw MissingMaterialError Attempted with no material bound.
    throw MissingMaterialError("Surface::material", "No material is bound");
}

Material *Surface::materialPtr() const
{
    return d->material;
}

Surface &Surface::setMaterial(Material *newMaterial, bool isMissingFix)
{
    // Sides of selfreferencing map lines should never receive fix materials.
    DE_ASSERT(!(isMissingFix && (parent().type() == DMU_SIDE && parent().as<LineSide>().line().isSelfReferencing())));

    if (d->material == newMaterial)
        return *this;

    d->materialIsMissingFix = false;
    d->material = newMaterial;
    if (d->material && isMissingFix)
    {
        d->materialIsMissingFix = true;
    }

    // During map setup we log missing material fixes.
    if (World::ddMapSetup && d->materialIsMissingFix && d->material)
    {
        if (d->owner().type() == DMU_SIDE)
        {
            auto &side = d->owner().as<LineSide>();
            dint section = (  this == &side.middle() ? LineSide::Middle
                            : this == &side.bottom() ? LineSide::Bottom
                            :                          LineSide::Top);

            LOGDEV_MAP_WARNING("%s of Line #%d is missing a material for the %s section."
                               "\n  %s was chosen to complete the definition.")
                << Line::sideIdAsText(side.sideId()).upperFirstChar()
                << side.line().indexInMap()
                << LineSide::sectionIdAsText(section)
                << d->material->manifest().composeUri().asText();
        }
    }

    resetLookups();

    // Notify interested parties.
    DE_NOTIFY(MaterialChange, i) i->surfaceMaterialChanged(*this);

    return *this;
}

const Vec2f &Surface::origin() const
{
    return d->origin;
}

Surface &Surface::setOrigin(const Vec2f &newOrigin)
{
    if (d->origin != newOrigin)
    {
        d->origin = newOrigin;
        DE_NOTIFY(OriginChange, i) i->surfaceOriginChanged(*this);
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

Vec2f Surface::materialScale() const
{
    return Vec2f(materialMirrorX()? -1 : 1, materialMirrorY()? -1 : 1);
}

res::Uri Surface::composeMaterialUri() const
{
    if (!hasMaterial()) return res::Uri();
    return material().manifest().composeUri();
}

void Surface::setDecorationState(IDecorationState *state)
{
    _decorationState.reset(state);
}

void world::Surface::resetLookups()
{}

float Surface::opacity() const
{
    return d->opacity;
}

Surface &Surface::setOpacity(float newOpacity)
{
    DE_ASSERT(Surface_isSideMiddle(*this) || Surface_isSectorExtraPlane(*this));  // sanity check

    newOpacity = de::clamp(0.f, newOpacity, 1.f);
    if (!de::fequal(d->opacity, newOpacity))
    {
        d->opacity = newOpacity;
        DE_NOTIFY(OpacityChange, i) i->surfaceOpacityChanged(*this);
    }
    return *this;
}

const Vec3f &Surface::color() const
{
    return d->color;
}

Surface &Surface::setColor(const Vec3f &newColor)
{
    Vec3f const newColorClamped(de::clamp(0.f, newColor.x, 1.f),
                                   de::clamp(0.f, newColor.y, 1.f),
                                   de::clamp(0.f, newColor.z, 1.f));

    if (d->color != newColorClamped)
    {
        d->color = newColorClamped;
        DE_NOTIFY(ColorChange, i) i->surfaceColorChanged(*this);
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

dint Surface::property(DmuArgs &args) const
{
    switch (args.prop)
    {
    case DMU_MATERIAL: {
        Material *mat = (d->materialIsMissingFix ? nullptr : d->material);
        args.setValue(DMT_SURFACE_MATERIAL, &mat, 0);
        break; }

    case DMU_OFFSET_X:
        args.setValue(DMT_SURFACE_OFFSET, &d->origin.x, 0);
        break;

    case DMU_OFFSET_Y:
        args.setValue(DMT_SURFACE_OFFSET, &d->origin.y, 0);
        break;

    case DMU_OFFSET_XY:
        args.setValue(DMT_SURFACE_OFFSET, &d->origin.x, 0);
        args.setValue(DMT_SURFACE_OFFSET, &d->origin.y, 1);
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
        args.setValue(DMT_SURFACE_RGBA, &d->color.x, 0);
        args.setValue(DMT_SURFACE_RGBA, &d->color.y, 1);
        args.setValue(DMT_SURFACE_RGBA, &d->color.z, 2);
        args.setValue(DMT_SURFACE_RGBA, &d->opacity, 2);
        break;

    case DMU_COLOR_RED:
        args.setValue(DMT_SURFACE_RGBA, &d->color.x, 0);
        break;

    case DMU_COLOR_GREEN:
        args.setValue(DMT_SURFACE_RGBA, &d->color.y, 0);
        break;

    case DMU_COLOR_BLUE:
        args.setValue(DMT_SURFACE_RGBA, &d->color.z, 0);
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

    return false;  // Continue iteration.
}

dint Surface::setProperty(const DmuArgs &args)
{
    switch (args.prop)
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
        Vec3f newColor = d->color;
        args.value(DMT_SURFACE_RGBA, &newColor.x, 0);
        args.value(DMT_SURFACE_RGBA, &newColor.y, 1);
        args.value(DMT_SURFACE_RGBA, &newColor.z, 2);
        setColor(newColor);
        break; }

    case DMU_COLOR_RED: {
        Vec3f newColor = d->color;
        args.value(DMT_SURFACE_RGBA, &newColor.x, 0);
        setColor(newColor);
        break; }

    case DMU_COLOR_GREEN: {
        Vec3f newColor = d->color;
        args.value(DMT_SURFACE_RGBA, &newColor.y, 0);
        setColor(newColor);
        break; }

    case DMU_COLOR_BLUE: {
        Vec3f newColor = d->color;
        args.value(DMT_SURFACE_RGBA, &newColor.z, 0);
        setColor(newColor);
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
        Vec2f newOrigin = d->origin;
        args.value(DMT_SURFACE_OFFSET, &newOrigin.x, 0);
        setOrigin(newOrigin);
        break; }

    case DMU_OFFSET_Y: {
        Vec2f newOrigin = d->origin;
        args.value(DMT_SURFACE_OFFSET, &newOrigin.y, 0);
        setOrigin(newOrigin);
        break; }

    case DMU_OFFSET_XY: {
        Vec2f newOrigin = d->origin;
        args.value(DMT_SURFACE_OFFSET, &newOrigin.x, 0);
        args.value(DMT_SURFACE_OFFSET, &newOrigin.y, 1);
        setOrigin(newOrigin);
        break; }

    default:
        return MapElement::setProperty(args);
    }

    return false;  // Continue iteration.
}

Surface::IDecorationState::~IDecorationState()
{}

} // namespace world
