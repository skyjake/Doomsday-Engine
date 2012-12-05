/**
 * @file patchname.h PatchName
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_PATCHNAME_H
#define LIBDENG_RESOURCE_PATCHNAME_H

#include "dd_types.h" // For lumpnum_t

#include <de/IReadable>
#include <de/Reader>
#include <de/String>

namespace de {

/**
 * @ingroup resource
 */
class PatchName : public IReadable
{
public:
    explicit PatchName(String percentEncodedName = "", lumpnum_t _lumpNum = -2);

    /// Returns the percent-endcoded symbolic name of the patch.
    String percentEncodedName() const;

    /// Returns the percent-endcoded symbolic name of the patch.
    String const &percentEncodedNameRef() const;

    /// Returns the lump number of the associated patch.
    /// @pre The global patchNames data is available.
    lumpnum_t lumpNum();

    /// Implements IReadable.
    void operator << (Reader &from);

private:
    String name;

    lumpnum_t lumpNum_;
};

} // namespace de

#endif /* LIBDENG_RESOURCE_PATCHNAME_H */
