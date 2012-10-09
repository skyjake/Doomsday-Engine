/**
 * @file fileid.h
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

#ifndef LIBDENG_FILEID_H
#define LIBDENG_FILEID_H

#include "fs_util.h"
#include <de/Log>
#include <de/str.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QString>

namespace de {

/**
 * File identifier (an MD5 hash).
 */
class FileId : public LogEntry::Arg::Base
{
public:
    typedef QByteArray Md5Hash;

public:
    explicit FileId(Md5Hash _md5) : md5_(_md5.left(16))
    {}

    FileId(FileId const& other)
        : md5_(other.md5())
    {}

    FileId& operator = (FileId other)
    {
        swap(*this, other);
        return *this;
    }

    /// @return @c true= this FileId is lexically less than @a other.
    bool operator < (FileId const& other) const { return md5_ < other.md5_; }

    /// @return @c true= this FileId is equal to @a other (identical hashes).
    bool operator == (FileId const& other) const { return md5_ == other.md5_; }

    /// @return @c true= this FileId is not equal @a other (differing hashes).
    bool operator != (FileId const& other) const { return md5_ != other.md5_; }

    friend void swap(FileId& first, FileId& second) // nothrow
    {
#ifdef DENG2_QT_4_8_OR_NEWER
        first.md5_.swap(second.md5_);
#else
        using std::swap;
        swap(first.md5_, second.md5_);
#endif
    }

    /// Converts this FileId to a text string.
    operator String () const { return asText(); }

    /// Converts this FileId to a text string.
    String asText() const
    {
        String txt;
        txt.reserve(32);
        for(int i = 0; i < 16; ++i)
        {
            txt += String("%1").arg(String::number((byte)md5_.at(i), 16), 2, '0');
        }
        return txt;
    }

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::STRING; }

    /// @return Md5hash for this FileId.
    Md5Hash const& md5() const { return md5_; }

    /**
     * Constructs a new FileId instance by hashing the absolute @a path.
     * @param path  Path to be hashed.
     * @return Newly construced FileId.
     */
    static FileId fromPath(char const* path)
    {
        return FileId(hash(path));
    }

    /**
     * Calculate an MD5 identifier for the absolute @a path.
     * @return MD5 hash of the path.
     */
    static Md5Hash hash(char const* path)
    {
        // First normalize the name.
        AutoStr* absPath = Str_Set(AutoStr_NewStd(), path);
        F_MakeAbsolute(absPath, absPath);
        F_FixSlashes(absPath, absPath);
#if defined(WIN32) || defined(MACOSX)
        // This is a case insensitive operation.
        strupr(Str_Text(absPath));
#endif
        return QCryptographicHash::hash(QByteArray(Str_Text(absPath)), QCryptographicHash::Md5);
    }

private:
    Md5Hash md5_;
};

} // namespace de

#endif /* LIBDENG_FILEID_H */
