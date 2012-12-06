/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/NativeFile"
#include "de/Guard"
#include "de/math.h"

using namespace de;

NativeFile::NativeFile(String const &name, NativePath const &nativePath)
    : ByteArrayFile(name), _nativePath(nativePath), _in(0), _out(0)
{}

NativeFile::~NativeFile()
{
    DENG2_GUARD(this);

    DENG2_FOR_AUDIENCE(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion.clear();
    
    close();
    deindex();
}

String NativeFile::describe() const
{
    return String("native file '%1'").arg(_nativePath.pretty());
}

void NativeFile::close()
{
    DENG2_GUARD(this);

    flush();
    if(_in)
    {
        delete _in;
        _in = 0;
    }
    if(_out)
    {
        delete _out;
        _out = 0;
    }
}

void NativeFile::flush()
{
    DENG2_GUARD(this);

    if(_out)
    {
        _out->flush();
    }
}

void NativeFile::clear()
{
    DENG2_GUARD(this);

    File::clear();
    
    Flags oldMode = mode();
    setMode(Write | Truncate);
    output();
    File::setMode(oldMode);
}

NativeFile::Size NativeFile::size() const
{
    DENG2_GUARD(this);

    return status().size;
}

void NativeFile::get(Offset at, Byte *values, Size count) const
{
    DENG2_GUARD(this);

    QFile &in = input();
    if(at + count > size())
    {
        /// @throw IByteArray::OffsetError  The region specified for reading extends
        /// beyond the bounds of the file.
        throw OffsetError("NativeFile::get", "Cannot read past end of file");
    }
    in.seek(at);
    in.read(reinterpret_cast<char *>(values), count);
}

void NativeFile::set(Offset at, Byte const *values, Size count)
{
    DENG2_GUARD(this);

    QFile &out = output();
    if(at > size())
    {
        /// @throw IByteArray::OffsetError  @a at specified a position beyond the
        /// end of the file.
        throw OffsetError("NativeFile::set", "Cannot write past end of file");
    }
    out.seek(at);
    out.write(reinterpret_cast<char const *>(values), count);
    if(out.error() != QFile::NoError)
    {
        /// @throw OutputError  Failure to write to the native file.
        throw OutputError("NativeFile::set", "Error writing to file:" +
                          out.errorString());
    }
    // Update status.
    Status st = status();
    st.size = max(st.size, at + count);
    st.modifiedAt = Time();
    setStatus(st);
}

void NativeFile::setMode(Flags const &newMode)
{
    DENG2_GUARD(this);

    close();
    File::setMode(newMode);
}

QFile &NativeFile::input() const
{
    DENG2_GUARD(this);

    if(!_in)
    {
        // Reading is allowed always.
        _in = new QFile(_nativePath);
        if(!_in->open(QFile::ReadOnly))
        {
            delete _in;
            _in = 0;
            /// @throw InputError  Opening the input stream failed.
            throw InputError("NativeFile::input", "Failed to read " + _nativePath);
        }
    }
    return *_in;
}

QFile &NativeFile::output()
{
    DENG2_GUARD(this);

    if(!_out)
    {
        // Are we allowed to output?
        verifyWriteAccess();
        
        QFile::OpenMode fileMode = QFile::ReadWrite;
        if(mode() & Truncate)
        {
            fileMode |= QFile::Truncate;
        }
        _out = new QFile(_nativePath);
        if(!_out->open(fileMode))
        {
            delete _out;
            _out = 0;
            /// @throw OutputError  Opening the output stream failed.
            throw OutputError("NativeFile::output", "Failed to write " + _nativePath);
        }
        if(mode() & Truncate)
        {
            Status st = status();
            st.size = 0;
            st.modifiedAt = Time();
            setStatus(st);
        }
    }
    return *_out;
}
