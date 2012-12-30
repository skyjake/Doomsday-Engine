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
#include "resource/materials.h"

namespace de {

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
        MaterialBind(MaterialScheme::Index::Node &_direcNode, materialid_t id)
            : direcNode(&_direcNode), asocMaterial(0), guid(id), extInfo(0)
        {}

        ~MaterialBind()
        {
            MaterialBindInfo *detachedInfo = detachInfo();
            if(detachedInfo) M_Free(detachedInfo);
        }

        /// @return  Unique identifier associated with this.
        materialid_t id() const { return guid; }

        /// @return  Index node associated with this.
        MaterialScheme::Index::Node &directoryNode() const { return *direcNode; }

        /// @return  Material associated with this else @c NULL.
        material_t *material() const { return asocMaterial; }

        /// @return  Extended info owned by this else @c NULL.
        MaterialBindInfo *info() const { return extInfo; }

        /**
         * Attach extended info data to this. If existing info is present it is replaced.
         * MaterialBind is given ownership of the info.
         * @param info  Extended info data to attach.
         */
        MaterialBind &attachInfo(MaterialBindInfo &info);

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
         * @return  This instance.
         */
        MaterialBind &setMaterial(material_t *material);

        /// @return  Detail texture definition associated with this else @c NULL
        ded_detailtexture_t *detailTextureDef() const;

        /// @return  Decoration definition associated with this else @c NULL
        ded_decor_t *decorationDef() const;

        /// @return  Particle generator definition associated with this else @c NULL
        ded_ptcgen_t *ptcGenDef() const;

        /// @return  Reflection definition associated with this else @c NULL
        ded_reflection_t *reflectionDef() const;

    private:
        /// This binding's node in the directory.
        MaterialScheme::Index::Node *direcNode;

        /// Material associated with this.
        material_t *asocMaterial;

        /// Unique identifier.
        materialid_t guid;

        /// Extended info about this binding. Will be attached upon successfull preparation
        /// of the first derived variant of the associated Material.
        MaterialBindInfo *extInfo;
    };

} // namespace de

#endif

#endif /* LIBDENG_RESOURCE_MATERIALBIND_H */
