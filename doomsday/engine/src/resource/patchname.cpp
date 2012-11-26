/**
 * @file patchname.cpp PatchName
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

#include <de/IReadable>
#include <de/Reader>
#include <de/String>
#include "filesys/fs_main.h"

#include "resource/patchname.h"

namespace de {

PatchName::PatchName(String _name, lumpnum_t _lumpNum)
    : name_(_name), lumpNum_(_lumpNum)
{}

lumpnum_t PatchName::lumpNum()
{
    // Have we already searched for this lump?
    if(lumpNum_ == -2)
    {
        // Mark as not found.
        lumpNum_ = -1;
        // Perform the search.
        try
        {
            lumpNum_ = App_FileSystem()->lumpNumForName(name_);
        }
        catch(FS1::NotFoundError const &er)
        {
            // Log but otherwise ignore this error.
            LOG_WARNING(er.asText() + ", ignoring.");
        }
    }
    return lumpNum_;
}

void PatchName::operator << (Reader &from)
{
    // The raw ASCII name is not necessarily terminated.
    char asciiName[9];
    for(int i = 0; i < 8; ++i) { from >> asciiName[i]; }
    asciiName[8] = 0;

    // WAD format allows characters not normally permitted in native paths.
    // To achieve uniformity we apply a percent encoding to the "raw" names.
    name_ = QString(QByteArray(asciiName).toPercentEncoding());

    // The cached found lump number is no longer valid.
    lumpNum_ = -2;
}

String PatchName::name() const {
    return name_;
}

String const &PatchName::nameRef() const {
    return name_;
}

} // namespace de
