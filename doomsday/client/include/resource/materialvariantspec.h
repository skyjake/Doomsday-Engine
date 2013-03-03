/** @file materialvariantspec.h Specialization specification for a variant material.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIALVARIANTSPEC_H
#define LIBDENG_RESOURCE_MATERIALVARIANTSPEC_H

#ifndef __CLIENT__
#  error "resource/materialvariantspec.h only exists in the Client"
#endif

#include "def_data.h"
#include "MaterialContext"
#include "Texture" // TextureVariantSpec

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
    MaterialContextId context;

    /// Specification for the primary texture.
    texturevariantspecification_t *primarySpec;

public:
    /**
     * Construct a zeroed MaterialVariantSpec instance.
     */
    MaterialVariantSpec() : context(MaterialContextId(0)), primarySpec(0)
    {}

    /**
     * Construct a MaterialVariantSpec instance by duplicating @a other.
     */
    MaterialVariantSpec(MaterialVariantSpec const &other)
        : context(other.context), primarySpec(other.primarySpec)
    {}

    /**
     * Determines whether specification @a other is equal to this specification.
     *
     * @param other  The other specification.
     * @return  @c true if specifications are equal; otherwise @c false.
     *
     * Same as operator ==
     */
    bool compare(MaterialVariantSpec const &other) const;

    /**
     * Determines whether specification @a other is equal to this specification.
     * @see compare()
     */
    bool operator == (MaterialVariantSpec const &other) const {
        return compare(other);
    }

    /**
     * Determines whether specification @a other is NOT equal to this specification.
     * @see compare()
     */
    bool operator != (MaterialVariantSpec const &other) const {
        return !(*this == other);
    }
};

} // namespace de

#endif /* LIBDENG_RESOURCE_MATERIALVARIANTSPEC_H */
