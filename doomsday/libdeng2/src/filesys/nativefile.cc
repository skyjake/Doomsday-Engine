/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/math.h"

using namespace de;

NativeFile::NativeFile(const String& name, const String& nativePath)
    : File(name), _nativePath(nativePath), _in(0), _out(0)
{}

NativeFile::~NativeFile()
{
    FOR_AUDIENCE(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion.clear();
    
    close();
    deindex();
}

void NativeFile::close()
{
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
    if(_out)
    {
        _out->flush();
    }
}

void NativeFile::clear()
{
    File::clear();
    
    Mode oldMode = mode();
    setMode(WRITE | TRUNCATE);
    output();
    File::setMode(oldMode);
}

NativeFile::Size NativeFile::size() const
{
    return status().size;
}

void NativeFile::get(Offset at, Byte* values, Size count) const
{
    std::ifstream& is = input();
    if(at + count > size())
    {
        /// @throw IByteArray::OffsetError  The region specified for reading extends
        /// beyond the bounds of the file.
        throw OffsetError("NativeFile::get", "Cannot read past end of file");
    }
    is.seekg(at);
    is.read(reinterpret_cast<char*>(values), count);
}

void NativeFile::set(Offset at, const Byte* values, Size count)
{
    std::ofstream& os = output();
    if(at > size())
    {
        /// @throw IByteArray::OffsetError  @a at specified a position beyond the
        /// end of the file.
        throw OffsetError("NativeFile::set", "Cannot write past end of file");
    }
    os.seekp(at);
    os.write(reinterpret_cast<const char*>(values), count);
    if(!os.good())
    {
        throw OutputError("NativeFile::set", "Error writing to stream");
    }
    // Update status.
    Status st = status();
    st.size = max(st.size, at + count);
    st.modifiedAt = Time();
    setStatus(st);
}

void NativeFile::setMode(const Mode& newMode)
{
    close();
    File::setMode(newMode);
}

std::ifstream& NativeFile::input() const
{
    if(!_in)
    {
        // Reading is allowed always.
        _in = new std::ifstream(_nativePath.c_str(), std::ifstream::binary | std::ifstream::in);
        if(!_in->good())
        {
            delete _in;
            _in = 0;
            /// @throw InputError  Opening the input stream failed.
            throw InputError("NativeFile::input", "Failed to read " + _nativePath);
        }
    }
    return *_in;
}

std::ofstream& NativeFile::output()
{
    if(!_out)
    {
        // Are we allowed to output?
        verifyWriteAccess();
        
        std::ios::openmode bits = std::ios::binary | std::ios::out;
        if(mode()[TRUNCATE_BIT])
        {
            bits |= std::ios::trunc;
        }
        _out = new std::ofstream(_nativePath.c_str(), bits);
        if(!_out->good())
        {
            delete _out;
            _out = 0;
            /// @throw OutputError  Opening the output stream failed.
            throw OutputError("NativeFile::output", "Failed to write " + _nativePath);
        }
        if(mode()[TRUNCATE_BIT])
        {
            Status st = status();
            st.size = 0;
            st.modifiedAt = Time();
            setStatus(st);
        }
    }
    return *_out;
}
