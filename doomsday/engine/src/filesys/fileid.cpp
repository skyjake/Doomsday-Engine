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

#include <QDir>
#include <QCryptographicHash>

#include "de_platform.h"
#include <de/App>
#include <de/Log>

#include "filesys/fileid.h"

using namespace de;

FileId::FileId(Md5Hash _md5) : md5_(_md5.left(16))
#ifdef DENG_DEBUG
  , path_("unknown-path")
#endif
{}

FileId::FileId(FileId const& other) : LogEntry::Arg::Base(), md5_(other.md5())
#ifdef DENG_DEBUG
  , path_(other.path())
#endif
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

FileId FileId::fromPath(String path)
{
    FileId fileId = FileId(hash(path));
#ifdef DENG_DEBUG
    fileId.setPath(path);
#endif
    return fileId;
}

FileId::Md5Hash FileId::hash(String path)
{
    // Ensure we've a normalized path.
    if(QDir::isRelativePath(path))
    {
        String basePath = DENG2_APP->nativeBasePath().withSeparators('/');
        path = basePath / path;
    }

#if defined(WIN32) || defined(MACOSX)
    // This is a case insensitive operation.
    path = path.toUpper();
#endif
    return QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5);
}
