/**
 * @file patchname.h PatchName
 *
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_RESOURCE_PATCHNAME_H
#define LIBDOOMSDAY_RESOURCE_PATCHNAME_H

#include "dd_types.h" // For lumpnum_t

#include <de/ireadable.h>
#include <de/reader.h>
#include <de/string.h>

namespace res {

/**
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC PatchName : public de::IReadable
{
public:
    explicit PatchName(de::String percentEncodedName = "", lumpnum_t lumpNum = -2);

    /// Returns the percent-endcoded symbolic name of the patch.
    de::String percentEncodedName() const;

    /// Returns the percent-endcoded symbolic name of the patch.
    const de::String &percentEncodedNameRef() const;

    /// Returns the lump number of the associated patch.
    /// @pre The global patchNames data is available.
    lumpnum_t lumpNum() const;

    /// Implements IReadable.
    void operator << (de::Reader &from);

private:
    de::String _name;
    mutable lumpnum_t _lumpNum;
};

} // namespace res

#endif /* LIBDOOMSDAY_RESOURCE_PATCHNAME_H */
