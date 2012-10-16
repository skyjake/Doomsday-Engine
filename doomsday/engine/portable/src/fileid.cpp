/**
 * @file fileid.cpp
 *
 * Implements a file identifier in terms of a MD5 hash of its absolute path.
 *
 * @ingroup types
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

#include <QCryptographicHash>
#include <de/Log>

#include "de_platform.h"
#include "fs_util.h"

#include "fileid.h"

using namespace de;

FileId::FileId(Md5Hash _md5) : md5_(_md5.left(16))
{}

FileId::FileId(FileId const& other) : LogEntry::Arg::Base(), md5_(other.md5())
{}

FileId& FileId::operator = (FileId other)
{
    swap(*this, other);
    return *this;
}

bool FileId::operator < (FileId const& other) const
{
    return md5_ < other.md5_;
}

bool FileId::operator == (FileId const& other) const
{
    return md5_ == other.md5_;
}

bool FileId::operator != (FileId const& other) const
{
    return md5_ != other.md5_;
}

String FileId::asText() const
{
    String txt;
    txt.reserve(32);
    for(int i = 0; i < 16; ++i)
    {
        txt += String("%1").arg(String::number((byte)md5_.at(i), 16), 2, '0');
    }
    return txt;
}

FileId FileId::fromPath(char const* path)
{
    return FileId(hash(path));
}

FileId::Md5Hash FileId::hash(char const* path)
{
    // First normalize the name.
    AutoStr* absPath = AutoStr_FromTextStd(path);
    F_MakeAbsolute(absPath, absPath);
    F_FixSlashes(absPath, absPath);
#if defined(WIN32) || defined(MACOSX)
    // This is a case insensitive operation.
    strupr(Str_Text(absPath));
#endif
    return QCryptographicHash::hash(QByteArray(Str_Text(absPath)), QCryptographicHash::Md5);
}
