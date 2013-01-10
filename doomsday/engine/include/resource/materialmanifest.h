/** @file materialmanifest.h Material Manifest.
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

#ifndef LIBDENG_RESOURCE_MATERIALMANIFEST_H
#define LIBDENG_RESOURCE_MATERIALMANIFEST_H

#ifdef __cplusplus

#include <de/Error>
#include <de/PathTree>
#include "def_data.h"
#include "uri.hh"
#include "resource/material.h"

namespace de {

class Materials;
class MaterialScheme;

class MaterialManifest : public PathTree::Node
{
public:
    /// Required material instance is missing. @ingroup errors
    DENG2_ERROR(MissingMaterialError);

public:
    MaterialManifest(PathTree::NodeArgs const &args);

    virtual ~MaterialManifest();

    void setId(materialid_t newId);
    void setCustom(bool yes);

    /**
     * Returns the owning scheme of the material manifest.
     */
    MaterialScheme &scheme() const;

    /// Convenience method for returning the name of the owning scheme.
    String const &schemeName() const;

    /**
     * Compose a URI of the form "scheme:path" for the material manifest.
     *
     * The scheme component of the URI will contain the symbolic name of
     * the scheme for the material manifest.
     *
     * The path component of the URI will contain the percent-encoded path
     * of the material manifest.
     */
    Uri composeUri(QChar sep = '/') const;

    /// @return  Unique identifier associated with this.
    materialid_t id() const;

    /// @return  @c true if the material manifest is not derived from an original game resource.
    bool isCustom() const;

    /// @return  Material associated with the manifest; otherwise @c NULL.
    material_t *material() const;

    /**
     * Change the material associated with this manifest.
     *
     * @post If @a material differs from that currently associated with this,
     *       any Info presently owned by this will destroyed (its invalid).
     *
     * @param  material  New material to associate with this.
     */
    void setMaterial(material_t *material);

    /**
     * Update the manifest with new linked definitions. Should be called:
     *
     * - When the bound material is changed/first-configured.
     * - When said material's "custom" state changes.
     */
    void linkDefinitions();

    /**
     * Zeroes all links to definitions. Should be called when the
     * definition database is reset.
     */
    void clearDefinitionLinks();

    /// @return  Detail texture definition associated with this else @c NULL
    ded_detailtexture_t *detailTextureDef() const;

    /// @return  Decoration definition associated with this else @c NULL
    ded_decor_t *decorationDef() const;

    /// @return  Particle generator definition associated with this else @c NULL
    ded_ptcgen_t *ptcGenDef() const;

    /// @return  Reflection definition associated with this else @c NULL
    ded_reflection_t *reflectionDef() const;

    /// Returns a reference to the application's material system.
    static Materials &materials();

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif

#endif /* LIBDENG_RESOURCE_MATERIALMANIFEST_H */
