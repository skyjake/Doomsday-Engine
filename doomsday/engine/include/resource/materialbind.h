/** @file materialbind.h Material Bind.
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

#ifndef LIBDENG_RESOURCE_MATERIALBIND_H
#define LIBDENG_RESOURCE_MATERIALBIND_H

#ifdef __cplusplus

#include "def_data.h"
#include "resource/materialscheme.h"

namespace de {

    class Materials;

    /**
     * Contains extended info about a material binding (see MaterialBind).
     * @note POD object.
     */
    struct MaterialBindInfo
    {
        ded_decor_t *decorationDefs[2];
        ded_detailtexture_t *detailtextureDefs[2];
        ded_ptcgen_t *ptcgenDefs[2];
        ded_reflection_t *reflectionDefs[2];
    };

    class MaterialBind
    {
    public:
        MaterialBind(MaterialScheme::Index::Node &_direcNode, materialid_t id);

        virtual ~MaterialBind();

        /**
         * Returns the owning scheme of the material bind.
         */
        MaterialScheme &scheme() const;

        /// Convenience method for returning the name of the owning scheme.
        String const &schemeName() const;

        /**
         * Compose a URI of the form "scheme:path" for the material bind.
         *
         * The scheme component of the URI will contain the symbolic name of
         * the scheme for the material bind.
         *
         * The path component of the URI will contain the percent-encoded path
         * of the material bind.
         */
        Uri composeUri(QChar sep = '/') const;

        /// @return  Unique identifier associated with this.
        materialid_t id() const;

        /// @return  Index node associated with this.
        MaterialScheme::Index::Node &directoryNode() const;

        /// @return  Material associated with this else @c NULL.
        material_t *material() const;

        /// @return  Extended info owned by this else @c NULL.
        MaterialBindInfo *info() const;

        /**
         * Attach extended info data to this. If existing info is present it is replaced.
         * MaterialBind is given ownership of the info.
         * @param info  Extended info data to attach.
         */
        void attachInfo(MaterialBindInfo &info);

        /**
         * Detach any extended info owned by this and relinquish ownership to the caller.
         * @return  Extended info or else @c NULL if not present.
         */
        MaterialBindInfo *detachInfo();

        /**
         * Change the Material associated with this binding.
         *
         * @note Only the relationship from MaterialBind to @a material changes!
         *
         * @post If @a material differs from that currently associated with this, any
         *       MaterialBindInfo presently owned by this will destroyed (its invalid).
         *
         * @param  material  New Material to associate with this.
         */
        void setMaterial(material_t *material);

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

#endif /* LIBDENG_RESOURCE_MATERIALBIND_H */
