#include <utility>

/** @file patchname.cpp PatchName
 *
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/patchname.h"
#include "doomsday/filesys/fs_main.h"

#include <de/ireadable.h>
#include <de/reader.h>
#include <de/string.h>

using namespace de;

namespace res {

PatchName::PatchName(String percentEncodedName, lumpnum_t lumpNum)
    : _name(std::move(percentEncodedName))
    , _lumpNum(lumpNum)
{}

lumpnum_t PatchName::lumpNum() const
{
    // Have we already searched for this lump?
    if (_lumpNum == -2)
    {
        // Mark as not found.
        _lumpNum = -1;
        // Perform the search.
        try
        {
            _lumpNum = App_FileSystem().lumpNumForName(_name);
        }
        catch (const FS1::NotFoundError &er)
        {
            // Log but otherwise ignore this error.
            LOG_RES_WARNING(er.asText() + ", ignoring.");
        }
    }
    return _lumpNum;
}

void PatchName::operator << (de::Reader &from)
{
    // The raw ASCII name is not necessarily terminated.
    char asciiName[9];
    for (int i = 0; i < 8; ++i) { from >> asciiName[i]; }
    asciiName[8] = 0;

    // WAD format allows characters not normally permitted in native paths.
    // To achieve uniformity we apply a percent encoding to the "raw" names.
    _name = String(asciiName).toPercentEncoding();

    // The cached found lump number is no longer valid.
    _lumpNum = -2;
}

String PatchName::percentEncodedName() const
{
    return _name;
}

const String &PatchName::percentEncodedNameRef() const
{
    return _name;
}

} // namespace res
