/** @file fileid.cpp
 *
 * Implements a file identifier in terms of a MD5 hash of its absolute path.
 *
 * @ingroup types
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <de/app.h>
#include <de/log.h>

#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fileid.h"

using namespace de;

FileId::FileId(const Md5Hash& _md5)
    : md5_(_md5.left(16))
#ifdef DE_DEBUG
    , path_("unknown-path")
#endif
{}

FileId::FileId(const FileId &other)
    : LogEntry::Arg::Base()
    , md5_(other.md5())
#ifdef DE_DEBUG
    , path_(other.path())
#endif
{}

FileId &FileId::operator = (FileId other)
{
    swap(*this, other);
    return *this;
}

bool FileId::operator < (const FileId &other) const
{
    return md5_ < other.md5_;
}

bool FileId::operator == (const FileId &other) const
{
    return md5_ == other.md5_;
}

bool FileId::operator != (const FileId &other) const
{
    return md5_ != other.md5_;
}

String FileId::asText() const
{
    return md5_.asHexadecimalText();
}

FileId FileId::fromPath(const String& path)
{
    FileId fileId = FileId(hash(path));
#ifdef DE_DEBUG
    fileId.setPath(path);
#endif
    return fileId;
}

FileId::Md5Hash FileId::hash(String path)
{
    // Ensure we've a normalized path.
    path = App_BasePath() / path;

#if defined(DE_WINDOWS) || defined(MACOSX)
    // This is a case insensitive operation.
    path = path.upper();
#endif

    return Block(path).md5Hash();
}
