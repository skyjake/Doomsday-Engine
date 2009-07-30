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

NativeFile::NativeFile(const String& name, const String& nativePath, const Mode& m)
    : File(name), nativePath_(nativePath), mode_(m), in_(0), out_(0)
{}

NativeFile::~NativeFile()
{
    close();
    deindex();
}

void NativeFile::close()
{
    if(in_)
    {
        delete in_;
        in_ = 0;
    }
    if(out_)
    {
        delete out_;
        out_ = 0;
    }
}

void NativeFile::clear()
{
    if(!mode_[WRITE_BIT])
    {
        /// @throw ReadOnlyError  Mode flags allow reading only.
        throw ReadOnlyError("NativeFile::clear", "Only reading allowed");
    }
    
    Mode oldMode = mode_;
    close();
    mode_.set(TRUNCATE_BIT);
    output();
    mode_ = oldMode;
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
    mode_ = newMode;
}

std::ifstream& NativeFile::input() const
{
    if(!in_)
    {
        // Reading is allowed always.
        in_ = new std::ifstream(nativePath_.c_str(), std::ifstream::binary | std::ifstream::in);
        if(!in_->good())
        {
            delete in_;
            in_ = 0;
            /// @throw InputError  Opening the input stream failed.
            throw InputError("NativeFile::input", "Failed to read " + nativePath_);
        }
    }
    return *in_;
}

std::ofstream& NativeFile::output()
{
    if(!out_)
    {
        // Are we allowed to output?
        if(!mode_[WRITE_BIT])
        {
            /// @throw ReadOnlyError  Mode flags allow reading only.
            throw ReadOnlyError("NativeFile::output", "Only reading allowed");
        }
        
        std::ios::openmode bits = std::ios::binary | std::ios::out;
        if(mode_[TRUNCATE_BIT])
        {
            bits |= std::ios::trunc;
        }
        out_ = new std::ofstream(nativePath_.c_str(), bits);
        if(!out_->good())
        {
            delete out_;
            out_ = 0;
            /// @throw OutputError  Opening the output stream failed.
            throw OutputError("NativeFile::output", "Failed to write " + nativePath_);
        }
        if(mode_[TRUNCATE_BIT])
        {
            Status st = status();
            st.size = 0;
            st.modifiedAt = Time();
            setStatus(st);
        }
    }
    return *out_;
}
