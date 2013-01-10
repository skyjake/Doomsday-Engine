/** @file materialmanifest.cpp Material Manifest.
 *
 * @author Copyright &copy; 2011-2013 Daniel Swanson <danij@dengine.net>
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
#include "de_defs.h"
#include <de/memory.h>

#include "uri.hh"
#include "resource/materials.h"
#include "resource/materialmanifest.h"

namespace de {

MaterialManifest::Info::Info()
{
    clearDefinitionLinks();
}

void MaterialManifest::Info::linkDefinitions(MaterialManifest const &manifest)
{
    material_t *mat = manifest.material();

    // Surface decorations (lights and models).
    decorationDefs[0]    = Def_GetDecoration(mat, 0, manifest.isCustom());
    decorationDefs[1]    = Def_GetDecoration(mat, 1, manifest.isCustom());

    // Reflection (aka shiny surface).
    reflectionDefs[0]    = Def_GetReflection(mat, 0, manifest.isCustom());
    reflectionDefs[1]    = Def_GetReflection(mat, 1, manifest.isCustom());

    // Generator (particles).
    ptcgenDefs[0]        = Def_GetGenerator(mat, 0, manifest.isCustom());
    ptcgenDefs[1]        = Def_GetGenerator(mat, 1, manifest.isCustom());

    // Detail texture.
    detailtextureDefs[0] = Def_GetDetailTex(mat, 0, manifest.isCustom());
    detailtextureDefs[1] = Def_GetDetailTex(mat, 1, manifest.isCustom());
}

void MaterialManifest::Info::clearDefinitionLinks()
{
    decorationDefs[0]    = decorationDefs[1]    = 0;
    detailtextureDefs[0] = detailtextureDefs[1] = 0;
    ptcgenDefs[0]        = ptcgenDefs[1]        = 0;
    reflectionDefs[0]    = reflectionDefs[1]    = 0;
}

struct MaterialManifest::Instance
{
    /// Material associated with this.
    material_t *material;

    /// Unique identifier.
    materialid_t id;

    /// Extended info for the manifest. Will be attached upon successfull preparation
    /// of the first derived variant of the associated Material.
    MaterialManifest::Info *extInfo;

    /// @c true if the material is not derived from an original game resource.
    bool isCustom;
};

MaterialManifest::MaterialManifest(PathTree::NodeArgs const &args)
    : Node(args)
{
    d = new Instance();
}

MaterialManifest::~MaterialManifest()
{
    Info *detachedInfo = detachInfo();
    if(detachedInfo) delete detachedInfo;
    delete d;
}

Materials &MaterialManifest::materials()
{
    return *App_Materials();
}

void MaterialManifest::setId(materialid_t id)
{
    d->id = id;
}

void MaterialManifest::setCustom(bool yes)
{
    d->isCustom = yes;
}

MaterialScheme &MaterialManifest::scheme() const
{
    LOG_AS("MaterialManifest::scheme");
    /// @todo Optimize: MaterialManifest should contain a link to the owning MaterialScheme.
    Materials::Schemes const &schemes = materials().allSchemes();
    DENG2_FOR_EACH_CONST(Materials::Schemes, i, schemes)
    {
        MaterialScheme &scheme = **i;
        if(&scheme.index() == &tree()) return scheme;
    }

    // This should never happen...
    /// @throw Error Failed to determine the scheme of the manifest.
    throw Error("MaterialManifest::scheme", String("Failed to determine scheme for manifest [%p].").arg(de::dintptr(this)));
}

String const &MaterialManifest::schemeName() const
{
    return scheme().name();
}

Uri MaterialManifest::composeUri(QChar sep) const
{
    return Uri(schemeName(), path(sep));
}

materialid_t MaterialManifest::id() const
{
    return d->id;
}

bool MaterialManifest::isCustom() const
{
    return d->isCustom;
}

material_t *MaterialManifest::material() const
{
    return d->material;
}

MaterialManifest::Info *MaterialManifest::info() const
{
    return d->extInfo;
}

void MaterialManifest::setMaterial(material_t *newMaterial)
{
    if(d->material != newMaterial)
    {
        // Any extended info will be invalid after this op, so destroy it
        // (it will automatically be rebuilt later, if subsequently needed).
        MaterialManifest::Info *detachedInfo = detachInfo();
        if(detachedInfo) delete detachedInfo;

        // Associate with the new Material.
        d->material = newMaterial;
    }
}

void MaterialManifest::attachInfo(MaterialManifest::Info &info)
{
    LOG_AS("MaterialManifest::attachInfo");
    if(d->extInfo)
    {
#if _DEBUG
        LOG_DEBUG("Info already present for \"%s\", will replace.") << composeUri(d->id);
#endif
        M_Free(d->extInfo);
    }
    d->extInfo = &info;
}

MaterialManifest::Info *MaterialManifest::detachInfo()
{
    Info *retInfo = d->extInfo;
    d->extInfo = 0;
    return retInfo;
}

ded_detailtexture_t *MaterialManifest::detailTextureDef() const
{
    if(!d->extInfo || !d->material || !Material_Prepared(d->material)) return 0;
    return d->extInfo->detailtextureDefs[Material_Prepared(d->material) - 1];
}

ded_decor_t *MaterialManifest::decorationDef() const
{
    if(!d->extInfo || !d->material || !Material_Prepared(d->material)) return 0;
    return d->extInfo->decorationDefs[Material_Prepared(d->material) - 1];
}

ded_ptcgen_t *MaterialManifest::ptcGenDef() const
{
    if(!d->extInfo || !d->material || !Material_Prepared(d->material)) return 0;
    return d->extInfo->ptcgenDefs[Material_Prepared(d->material) - 1];
}

ded_reflection_t *MaterialManifest::reflectionDef() const
{
    if(!d->extInfo || !d->material || !Material_Prepared(d->material)) return 0;
    return d->extInfo->reflectionDefs[Material_Prepared(d->material)-1];
}

} // namespace de
