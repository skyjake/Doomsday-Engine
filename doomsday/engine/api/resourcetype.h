/**
 * @file resourcetype.h
 *
 * Resource Type.
 *
 * @ingroup core
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

#ifndef LIBDENG_RESOURCETYPE_H
#define LIBDENG_RESOURCETYPE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resource Type identifer attributable to resources (e.g., files).
 *
 * @ingroup core
 */
typedef enum resourcetypeid_e {
    RT_NONE = 0,
    RT_FIRST = 1,
    RT_ZIP = RT_FIRST,
    RT_WAD,
    RT_LMP,
    RT_DED,
    RT_PNG,
    RT_JPG,
    RT_TGA,
    RT_PCX,
    RT_DMD,
    RT_MD2,
    RT_WAV,
    RT_OGG,
    RT_MP3,
    RT_MOD,
    RT_MID,
    RT_DEH,
    RT_DFN,
    RT_LAST_INDEX
} resourcetypeid_t;

#define RESOURCETYPE_COUNT          (RT_LAST_INDEX - 1)
#define VALID_RESOURCE_TYPEID(v)    ((v) >= RT_FIRST && (v) < RT_LAST_INDEX)

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#ifndef DENG2_C_API_ONLY

#include <QStringList>
#include <de/String>

enum resourceclassid_e;

namespace de
{
    /**
     * ResourceType.
     *
     * @ingroup core
     */
    struct ResourceType
    {
        String name_;

        /// Default class attributed to resources of this type.
        resourceclassid_e defaultClass_;

        /// List of known extensions for this resource type.
        QStringList knownFileNameExtensions;

        ResourceType(String _name, resourceclassid_e _defaultClass)
            : name_(_name), defaultClass_(_defaultClass)
        {}

        virtual ~ResourceType() {};

        String const& name() const {
            return name_;
        }

        resourceclassid_e defaultClass() const {
            return defaultClass_;
        }

        ResourceType& addKnownExtension(String ext) {
            knownFileNameExtensions.push_back(ext);
            return *this;
        }

        bool fileNameIsKnown(String path) const
        {
            // We require an extension for this.
            String ext = path.fileNameExtension();
            if(!ext.isEmpty())
            {
                return knownFileNameExtensions.contains(ext, Qt::CaseInsensitive);
            }
            return false;
        }
    };

    /**
     * The special "null" ResourceType object.
     *
     * @ingroup core
     */
    struct NullResourceType : public ResourceType
    {
        NullResourceType() : ResourceType("RT_NONE",  RC_UNKNOWN)
        {}
    };

    /// @return  @c true= @a rtype is a "null-resourcetype" object (not a real resource type).
    inline bool isNullResourceType(ResourceType const& rtype) {
        return !!dynamic_cast<NullResourceType const*>(&rtype);
    }

} // namespace de
#endif // DENG2_C_API_ONLY
#endif // __cplusplus

#endif /* LIBDENG_RESOURCETYPE_H */
