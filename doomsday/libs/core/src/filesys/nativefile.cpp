/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/nativefile.h"
#include "de/directoryfeed.h"
#include "de/guard.h"
#include "de/math.h"

namespace de {

DE_PIMPL(NativeFile)
{
    /// Path of the native file in the OS file system.
    NativePath nativePath;

    /// Input stream.
    mutable std::ifstream *in;

    /// Output stream. Kept open until flush() is called.
    /// (Re)opened before changing the contents of the file.
    std::ofstream *out;

    /// Output file should be truncated before the next write.
    bool needTruncation;

    Impl(Public *i)
        : Base(i)
        , in(nullptr)
        , out(nullptr)
        , needTruncation(false)
    {}

    ~Impl()
    {
        DE_ASSERT(!in);
        DE_ASSERT(!out);
    }

    std::ifstream &getInput()
    {
        if (!in)
        {
            // Reading is allowed always.
            in = new std::ifstream(nativePath, std::ios::binary);
            if (!*in)
            {
                delete in;
                in = nullptr;
                /// @throw InputError  Opening the input stream failed.
                throw InputError("NativeFile::openInput", "Failed to read " + nativePath);
            }
        }
        return *in;
    }

    std::ofstream &getOutput()
    {
        using namespace std;
        if (!out)
        {
            // Are we allowed to output?
            self().verifyWriteAccess();

            ios::openmode fileMode = ios::binary | ios::out;
            if (self().mode() & Truncate)
            {
                if (needTruncation)
                {
                    fileMode |= ios::trunc;
                    needTruncation = false;
                }
            }
            out = new std::ofstream{nativePath, fileMode};
            if (!*out)
            {
                delete out;
                out = nullptr;
                /// @throw OutputError  Opening the output stream failed.
                throw OutputError("NativeFile::output",
                                  "Failed to write " + nativePath + " (" + strerror(errno) + ")");
            }
            if (self().mode() & Truncate)
            {
                Status st = self().status();
                st.size = 0;
                st.modifiedAt = Time();
                self().setStatus(st);
            }
        }
        return *out;
    }

    void closeInput()
    {
        if (in)
        {
            delete in;
            in = 0;
        }
    }

    void closeOutput()
    {
        if (out)
        {
            delete out;
            out = 0;
        }
    }
};

NativeFile::NativeFile(const String &name, const NativePath &nativePath)
    : ByteArrayFile(name), d(new Impl(this))
{
    d->nativePath = nativePath;
}

NativeFile::~NativeFile()
{
    DE_GUARD(this);

    DE_NOTIFY(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();

    close();
    deindex();
}

String NativeFile::describe() const
{
    DE_GUARD(this);
    return Stringf("\"%s\"", d->nativePath.pretty().c_str());
}

Block NativeFile::metaId() const
{
    // Special exception: application's persistent data store will be rewritten on every
    // run so there's no point in caching it.
    if (name() == DE_STR("persist.pack"))
    {
        return Block();
    }
    return Block(File::metaId() + d->nativePath.toUtf8()).md5Hash();
}

void NativeFile::close()
{
    DE_GUARD(this);

    flush();
    DE_ASSERT(!d->out);

    d->closeInput();
}

void NativeFile::flush()
{
    DE_GUARD(this);

    d->closeOutput();
    DE_ASSERT(!d->out);
}

const NativePath &NativeFile::nativePath() const
{
    DE_GUARD(this);

    return d->nativePath;
}

void NativeFile::clear()
{
    DE_GUARD(this);

    File::clear();

    Flags oldMode = mode();
    setMode(Write | Truncate);
    d->getOutput();
    File::setMode(oldMode);
}

NativeFile::Size NativeFile::size() const
{
    DE_GUARD(this);

    return status().size;
}

void NativeFile::get(Offset at, Byte *values, Size count) const
{
    DE_GUARD(this);

    if (at + count > size())
    {
        d->closeInput();
        /// @throw IByteArray::OffsetError  The region specified for reading extends
        /// beyond the bounds of the file.
        throw OffsetError("NativeFile::get", description() + ": cannot read past end of file " +
                          Stringf("(%zu[+%zu] > %zu)", at, count, size()));
    }
    auto &in = input();
    if (in.tellg() != std::ifstream::pos_type(at)) in.seekg(at);
    in.read(reinterpret_cast<char *>(values), count);

    // Close the native input file after the full contents have been read.
    if (at + count == size())
    {
        d->closeInput();
    }
}

void NativeFile::set(Offset at, const Byte *values, Size count)
{
    DE_GUARD(this);

    auto &out = output();
    if (at > size())
    {
        /// @throw IByteArray::OffsetError  @a at specified a position beyond the
        /// end of the file.
        throw OffsetError("NativeFile::set", description() + ": cannot write past end of file");
    }
    out.seekp(at);
    out.write(reinterpret_cast<const char *>(values), count);
    if (out.bad())
    {
        /// @throw OutputError  Failure to write to the native file.
        throw OutputError("NativeFile::set", description() + ": error writing to file");
    }
    // Update status.
    Status st = status();
    st.size = max(st.size, at + count);
    st.modifiedAt = Time();
    setStatus(st);
}

NativeFile *NativeFile::newStandalone(const NativePath &nativePath)
{
    std::unique_ptr<NativeFile> file(new NativeFile(nativePath.fileName(), nativePath));
    if (nativePath.exists())
    {
        // Sync status with the real status.
        file->setStatus(DirectoryFeed::fileStatus(nativePath));
    }
    return file.release();
}

void NativeFile::setMode(const Flags &newMode)
{
    DE_GUARD(this);

    close();
    File::setMode(newMode);

    if (newMode.testFlag(Truncate))
    {
        d->needTruncation = true;
    }
}

std::ifstream &NativeFile::input() const
{
    DE_GUARD(this);

    return d->getInput();
}

std::ofstream &NativeFile::output()
{
    DE_GUARD(this);

    return d->getOutput();
}

} // namespace de
