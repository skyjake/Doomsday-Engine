/** @file materialbind.cpp Material Bind.
 *
 * @author Copyright &copy; 2011-2012 Daniel Swanson <danij@dengine.net>
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
#include <de/memory.h>

#include "uri.hh"
#include "resource/materials.h"
#include "resource/materialbind.h"

namespace de {

struct MaterialBind::Instance
{
    /// Material associated with this.
    material_t *material;

    /// Unique identifier.
    materialid_t guid;

    /// Extended info about this binding. Will be attached upon successfull preparation
    /// of the first derived variant of the associated Material.
    MaterialBind::Info *extInfo;

    Instance() : material(0), guid(0), extInfo(0)
    {}
};

MaterialBind::MaterialBind(PathTree::NodeArgs const &args)
    : Node(args)
{
    d = new Instance();
}

MaterialBind::~MaterialBind()
{
    Info *detachedInfo = detachInfo();
    if(detachedInfo) M_Free(detachedInfo);
    delete d;
}

Materials &MaterialBind::materials()
{
    return *App_Materials();
}

void MaterialBind::setId(materialid_t id)
{
    d->guid = id;
}

MaterialScheme &MaterialBind::scheme() const
{
    LOG_AS("MaterialBind::scheme");
    /// @todo Optimize: MaterialBind should contain a link to the owning MaterialScheme.
    Materials::Schemes const &schemes = materials().allSchemes();
    DENG2_FOR_EACH_CONST(Materials::Schemes, i, schemes)
    {
        MaterialScheme &scheme = **i;
        if(&scheme.index() == &tree()) return scheme;
    }

    // This should never happen...
    /// @throw Error Failed to determine the scheme of the manifest.
    throw Error("MaterialBind::scheme", String("Failed to determine scheme for bind [%p].").arg(de::dintptr(this)));
}

String const &MaterialBind::schemeName() const
{
    return scheme().name();
}

Uri MaterialBind::composeUri(QChar sep) const
{
    return Uri(schemeName(), path(sep));
}

materialid_t MaterialBind::id() const
{
    return d->guid;
}

material_t *MaterialBind::material() const
{
    return d->material;
}

MaterialBind::Info *MaterialBind::info() const
{
    return d->extInfo;
}

void MaterialBind::setMaterial(material_t *newMaterial)
{
    if(d->material != newMaterial)
    {
        // Any extended info will be invalid after this op, so destroy it
        // (it will automatically be rebuilt later, if subsequently needed).
        MaterialBind::Info *detachedInfo = detachInfo();
        if(detachedInfo) M_Free(detachedInfo);

        // Associate with the new Material.
        d->material = newMaterial;
    }
}

void MaterialBind::attachInfo(MaterialBind::Info &info)
{
    LOG_AS("MaterialBind::attachInfo");
    if(d->extInfo)
    {
#if _DEBUG
        LOG_DEBUG("Info already present for \"%s\", will replace.") << composeUri(d->guid);
#endif
        M_Free(d->extInfo);
    }
    d->extInfo = &info;
}

MaterialBind::Info *MaterialBind::detachInfo()
{
    Info *retInfo = d->extInfo;
    d->extInfo = 0;
    return retInfo;
}

ded_detailtexture_t *MaterialBind::detailTextureDef() const
{
    if(!d->extInfo || !d->material || !Material_Prepared(d->material)) return 0;
    return d->extInfo->detailtextureDefs[Material_Prepared(d->material) - 1];
}

ded_decor_t *MaterialBind::decorationDef() const
{
    if(!d->extInfo || !d->material || !Material_Prepared(d->material)) return 0;
    return d->extInfo->decorationDefs[Material_Prepared(d->material) - 1];
}

ded_ptcgen_t *MaterialBind::ptcGenDef() const
{
    if(!d->extInfo || !d->material || !Material_Prepared(d->material)) return 0;
    return d->extInfo->ptcgenDefs[Material_Prepared(d->material) - 1];
}

ded_reflection_t *MaterialBind::reflectionDef() const
{
    if(!d->extInfo || !d->material || !Material_Prepared(d->material)) return 0;
    return d->extInfo->reflectionDefs[Material_Prepared(d->material)-1];
}

} // namespace de
