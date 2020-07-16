/** @file materialvariantspec.h  Logical material, draw-context variant specification.
 *
 * @authors Copyright © 2011-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RESOURCE_MATERIALVARIANTSPEC_H
#define CLIENT_RESOURCE_MATERIALVARIANTSPEC_H

#ifndef __CLIENT__
#  error "resource/materialvariantspec.h only exists in the Client"
#endif

#include "render/materialcontext.h"
#include "clienttexture.h" // TextureVariantSpec

namespace de {

/**
 * Specialization specification for a variant material.
 *
 * Property values are public for user convenience.
 *
 * @see Material, MaterialVariant
 * @ingroup resource
 */
struct MaterialVariantSpec
{
public:
    /// Usage context identifier.
    MaterialContextId contextId { FirstMaterialContextId };

    /// Interned specification for the primary texture.
    const TextureVariantSpec *primarySpec = nullptr;

    MaterialVariantSpec() {}
    MaterialVariantSpec(const MaterialVariantSpec &other)
        : contextId  (other.contextId)
        , primarySpec(other.primarySpec)
    {}

    /**
     * Determines whether specification @a other is equal to this specification.
     *
     * @param other  The other specification.
     * @return  @c true if specifications are equal; otherwise @c false.
     *
     * Same as operator ==
     */
    bool compare(const MaterialVariantSpec &other) const {
        if(this == &other) return true;
        if(contextId != other.contextId) return false;
        return primarySpec == other.primarySpec;
    }

    /**
     * Determines whether specification @a other is equal to this specification.
     * @see compare()
     */
    bool operator == (const MaterialVariantSpec &other) const {
        return compare(other);
    }

    /**
     * Determines whether specification @a other is NOT equal to this specification.
     * @see compare()
     */
    bool operator != (const MaterialVariantSpec &other) const {
        return !(*this == other);
    }
};

} // namespace de

#endif  // CLIENT_RESOURCE_MATERIALVARIANTSPEC_H
