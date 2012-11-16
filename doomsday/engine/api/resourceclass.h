/**
 * @file resourceclass.h
 *
 * Resource Class.
 *
 * @ingroup base
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCECLASS_H
#define LIBDENG_RESOURCECLASS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resource Class Identifier.
 *
 * @ingroup base
 *
 * @todo Refactor away. These identifiers are no longer needed.
 */
typedef enum resourceclassid_e {
    RC_NULL = -2,           ///< Not a real class, used internally during resource locator init.
    RC_UNKNOWN = -1,        ///< Attempt to guess the class using heuristic evaluation of the path.
    RESOURCECLASS_FIRST = 0,
    RC_PACKAGE = RESOURCECLASS_FIRST,
    RC_DEFINITION,
    RC_GRAPHIC,
    RC_MODEL,
    RC_SOUND,
    RC_MUSIC,
    RC_FONT,
    RESOURCECLASS_COUNT
} resourceclassid_t;

#define VALID_RESOURCE_CLASSID(n)   ((n) >= RESOURCECLASS_FIRST && (n) < RESOURCECLASS_COUNT)

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#ifndef DENG2_C_API_ONLY

#include <QList>
#include <de/String>
#include "resourcetype.h"

namespace de
{
    /**
     * ResourceClass encapsulates the properties and logics belonging to a logical
     * class of resource (e.g., Graphic, Model, Sound, etc...)
     *
     * @ingroup base
     */
    struct ResourceClass
    {
    public:
        typedef QList<resourcetypeid_e> Types;

    public:
        ResourceClass(String _name, String _defaultNamespace)
            : name_(_name), defaultNamespace_(_defaultNamespace)
        {}

        virtual ~ResourceClass() {};

        /// Return the symbolic name of this resource class.
        String const& name() const
        {
            return name_;
        }

        /// Return the symbolic name of the default namespace for this class of resource.
        String const& defaultNamespace() const
        {
            return defaultNamespace_;
        }

        /// Return the number of resource types for this class.
        int resourceTypeCount() const
        {
            return searchTypeOrder.count();
        }

        /**
         * Add a new type of resource to this class. Earlier types have priority.
         *
         * @param rtype  Identifier of the resourceType to add.
         * @return  This instance.
         */
        ResourceClass& addResourceType(resourcetypeid_e rtype)
        {
            searchTypeOrder.push_back(rtype);
            return *this;
        }

        /**
         * Provides access to the resource type list for efficient iteration.
         *
         * @return  List of resource types of this class.
         */
        Types const& resourceTypes() const
        {
            return searchTypeOrder;
        }

    private:
        /// Symbolic name for this class.
        String name_;

        /// Symbolic name of the default namespace.
        String defaultNamespace_;

        /// Recognized resource types (in order of importance, left to right).
        Types searchTypeOrder;
    };

    /**
     * The special "null" ResourceClass object.
     *
     * @ingroup core
     */
    struct NullResourceClass : public ResourceClass
    {
        NullResourceClass() : ResourceClass("RC_NULL",  "")
        {}
    };

    /// @return  @c true= @a rclass is a "null-resourceclass" object (not a real resource class).
    inline bool isNullResourceClass(ResourceClass const& rclass) {
        return !!dynamic_cast<NullResourceClass const*>(&rclass);
    }

} // namespace de
#endif // DENG2_C_API_ONLY
#endif // __cplusplus

#endif /* LIBDENG_RESOURCECLASS_H */
