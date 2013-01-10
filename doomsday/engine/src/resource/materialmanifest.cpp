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

struct MaterialManifest::Instance
{
    /// Material associated with this.
    material_t *material;

    /// Unique identifier.
    materialid_t id;

    /// @c true if the material is not derived from an original game resource.
    bool isCustom;

    /// Linked definitions contain further property values.
    /// There are two links for each definition type, the first (index @c 0)
    /// for original game data and the second for external data.
    struct {
        ded_decor_t         *decors[2];
        ded_detailtexture_t *detailtextures[2];
        ded_ptcgen_t        *ptcgens[2];
        ded_reflection_t    *reflections[2];
    } defs;

    Instance() : material(0), id(0), isCustom(false)
    {}
};

MaterialManifest::MaterialManifest(PathTree::NodeArgs const &args)
    : Node(args)
{
    d = new Instance();
    clearDefinitionLinks();
}

MaterialManifest::~MaterialManifest()
{
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
    throw Error("MaterialManifest::scheme", String("Failed to determine scheme for manifest [%1]").arg(de::dintptr(this)));
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

void MaterialManifest::setMaterial(material_t *newMaterial)
{
    if(d->material == newMaterial) return;
    d->material = newMaterial;
}

void MaterialManifest::linkDefinitions()
{
    Uri _uri = composeUri();
    uri_s *uri = reinterpret_cast<uri_s *>(&_uri);

    // Surface decorations (lights and models).
    d->defs.decors[0]         = Def_GetDecoration(uri, 0, d->isCustom);
    d->defs.decors[1]         = Def_GetDecoration(uri, 1, d->isCustom);

    // Reflection (aka shiny surface).
    d->defs.reflections[0]    = Def_GetReflection(uri, 0, d->isCustom);
    d->defs.reflections[1]    = Def_GetReflection(uri, 1, d->isCustom);

    // Generator (particles).
    d->defs.ptcgens[0]        = Def_GetGenerator(uri, 0, d->isCustom);
    d->defs.ptcgens[1]        = Def_GetGenerator(uri, 1, d->isCustom);

    // Detail texture.
    d->defs.detailtextures[0] = Def_GetDetailTex(uri, 0, d->isCustom);
    d->defs.detailtextures[1] = Def_GetDetailTex(uri, 1, d->isCustom);
}

void MaterialManifest::clearDefinitionLinks()
{
    d->defs.decors[0]         = d->defs.decors[1]         = 0;
    d->defs.detailtextures[0] = d->defs.detailtextures[1] = 0;
    d->defs.ptcgens[0]        = d->defs.ptcgens[1]        = 0;
    d->defs.reflections[0]    = d->defs.reflections[1]    = 0;
}

ded_detailtexture_t *MaterialManifest::detailTextureDef() const
{
    if(!d->material)
    {
        /// @throw MissingMaterialError A material is required for this.
        throw MissingMaterialError("MaterialManifest::detailTextureDef", "Missing required material");
    }

    // We must prepare to determine which definition is in effect.
    byte prepared = Material_Prepared(d->material);
    if(prepared) return d->defs.detailtextures[prepared - 1];
    return 0;
}

ded_decor_t *MaterialManifest::decorationDef() const
{
    if(!d->material)
    {
        /// @throw MissingMaterialError A material is required for this.
        throw MissingMaterialError("MaterialManifest::decorationTextureDef", "Missing required material");
    }

    // We must prepare to determine which definition is in effect.
    byte prepared = Material_Prepared(d->material);
    if(prepared) return d->defs.decors[prepared - 1];
    return 0;
}

ded_ptcgen_t *MaterialManifest::ptcGenDef() const
{
    if(!d->material)
    {
        /// @throw MissingMaterialError A material is required for this.
        throw MissingMaterialError("MaterialManifest::ptcGenDef", "Missing required material");
    }

    // We must prepare to determine which definition is in effect.
    byte prepared = Material_Prepared(d->material);
    if(prepared) return d->defs.ptcgens[prepared - 1];
    return 0;
}

ded_reflection_t *MaterialManifest::reflectionDef() const
{
    if(!d->material)
    {
        /// @throw MissingMaterialError A material is required for this.
        throw MissingMaterialError("MaterialManifest::reflectionDef", "Missing required material");
    }

    // We must prepare to determine which definition is in effect.
    byte prepared = Material_Prepared(d->material);
    if(prepared) return d->defs.reflections[prepared - 1];
    return 0;
}

} // namespace de
